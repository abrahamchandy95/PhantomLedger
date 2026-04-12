from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

import numpy as np

import entities.models as models
from common.math import I32, as_int, cdf_pick
from common.persona_names import SALARIED
from common.transactions import Transaction
from entities.personas import PERSONAS
from math_models.counts import DEFAULT_RATES, weekday_multiplier
from math_models.seasonal import monthly_multiplier
from math_models.timing import sample_offsets
from transfers.factory import TransactionFactory
from transfers.screening import advance_book_through

from .channels import build_channel_cdf, route_channel_txn
from .dynamics import advance_all_daily, evolve_all_monthly
from .engine import GenerateRequest
from .state import (
    Event,
    Spender,
    build_day,
    build_spender,
    calculate_explore_p,
    count_liquidity_factor,
    sample_txn_count,
)

_FALLBACK_PERSONA = PERSONAS[SALARIED]


@dataclass(frozen=True, slots=True)
class _PreparedSpender:
    person_idx: int
    spender: Spender
    paydays: frozenset[int]
    initial_cash: float
    baseline_cash: float
    fixed_burden: float
    paycheck_sensitivity: float


def _prepare_spenders(request: GenerateRequest) -> list[_PreparedSpender]:
    """
    Precompute all deterministic, per-person values used by the day-to-day engine.

    This is intentionally RNG-free so it preserves exact generation behavior while
    removing repeated dict lookups, object construction, and numeric recomputation
    from the hot day/person loop.
    """
    prepared: list[_PreparedSpender] = []

    paydays_by_person = request.ctx.paydays_by_person
    fixed_monthly_burden = request.fixed_monthly_burden
    total_people = len(request.ctx.population.persons)

    for person_idx in range(total_people):
        spender = build_spender(request, person_idx)
        if spender is None:
            continue

        initial_cash = float(spender.persona.initial_balance)

        prepared.append(
            _PreparedSpender(
                person_idx=person_idx,
                spender=spender,
                paydays=paydays_by_person[person_idx],
                initial_cash=initial_cash,
                baseline_cash=max(150.0, initial_cash),
                fixed_burden=float(fixed_monthly_burden.get(spender.id, 0.0)),
                paycheck_sensitivity=float(spender.persona.paycheck_sensitivity),
            )
        )

    return prepared


def _is_month_boundary(
    day_index: int,
    prev_day_index: int,
    start_date: datetime,
) -> bool:
    if day_index == 0:
        return False
    prev = start_date + timedelta(days=prev_day_index)
    curr = start_date + timedelta(days=day_index)
    return curr.month != prev.month or curr.year != prev.year


def _liquidity_multiplier(
    request: GenerateRequest,
    *,
    days_since_payday: int,
    paycheck_sensitivity: float,
    available_cash: float,
    baseline_cash: float,
    fixed_monthly_burden: float,
) -> float:
    params = request.params
    if not params.enable_liquidity_gating:
        return 1.0

    relief_days = int(params.liquidity_relief_days)
    stress_start = int(params.liquidity_stress_start_day)
    stress_ramp = int(params.liquidity_stress_ramp_days)

    relief = 0.0
    if relief_days > 0 and days_since_payday <= relief_days:
        relief = float(relief_days - days_since_payday) / float(relief_days)

    stress_days = max(0, days_since_payday - stress_start)
    stress = min(1.0, float(stress_days) / float(max(1, stress_ramp)))

    cycle_down = stress * (0.35 + 0.95 * paycheck_sensitivity)
    cycle_up = relief * (0.06 + 0.10 * paycheck_sensitivity)

    cycle_mult = 1.0 - cycle_down + cycle_up
    cycle_mult = min(1.05, max(float(params.liquidity_floor), cycle_mult))

    cash_ref = max(150.0, float(baseline_cash))
    cash_ratio = max(0.0, min(2.0, float(available_cash) / cash_ref))
    cash_mult = min(1.10, max(0.0, 0.10 + cash_ratio))

    burden_ratio = max(0.0, min(2.0, float(fixed_monthly_burden) / cash_ref))
    burden_mult = max(0.30, 1.0 - 0.35 * burden_ratio)

    mult = cycle_mult * cash_mult * burden_mult
    return min(1.10, max(0.0, mult))


def _target_total_txns(request: GenerateRequest, num_spenders: int) -> float:
    """Configured realized day-to-day target across the current window."""
    return (
        float(request.merchants_cfg.txns_per_month)
        * float(num_spenders)
        * (float(request.days) / 30.0)
    )


def _latent_base_rate_for_target(
    spender: Spender,
    *,
    day: datetime,
    day_shock: float,
    target_realized_per_day: float,
    dynamics_multiplier: float,
    liquidity_multiplier: float,
) -> float:
    """
    Back-calibrate the latent base intensity needed to hit a realized
    per-person-day target under today's count-stage suppressors.

    This now inverts every multiplier that still affects count
    generation: persona mix, weekday effects, day shock, behavior
    dynamics, and soft liquidity shaping.
    """
    if target_realized_per_day <= 0.0:
        return 0.0

    persona: models.Persona = spender.persona
    suppression = (
        float(persona.rate_multiplier)
        * float(weekday_multiplier(day, DEFAULT_RATES))
        * float(day_shock)
        * max(0.0, float(dynamics_multiplier))
        * count_liquidity_factor(liquidity_multiplier)
    )

    if suppression <= 0.0:
        return 0.0

    return target_realized_per_day / suppression


@dataclass(slots=True)
class _Generator:
    request: GenerateRequest

    def run(self) -> list[Transaction]:
        if self.request.days <= 0:
            return []

        txf = TransactionFactory(rng=self.request.rng, infra=self.request.infra)
        channel_cdf = build_channel_cdf(self.request)

        prefer_billers_p = float(self.request.events.prefer_billers_p)
        per_person_daily_limit = int(self.request.events.max_per_person_per_day)
        person_limit = None if per_person_daily_limit <= 0 else per_person_daily_limit

        dynamics_cfg = self.request.params.dynamics
        seasonal_cfg = self.request.params.seasonal
        ctx = self.request.ctx
        dynamics = ctx.person_dynamics
        total_people = len(ctx.population.persons)

        prepared_spenders = _prepare_spenders(self.request)
        active_spenders = len(prepared_spenders)
        target_total_txns = _target_total_txns(self.request, active_spenders)

        txns: list[Transaction] = []
        days_since_payday: list[int] = [365] * total_people

        base_txns = self.request.base_txns
        base_txns.sort(key=lambda t: t.timestamp)

        screen_book = self.request.screen_book
        base_idx = 0

        total_person_days = active_spenders * self.request.days
        processed_person_days = 0

        for day_index in range(self.request.days):
            if _is_month_boundary(day_index, day_index - 1, self.request.start_date):
                evolve_all_monthly(
                    self.request.rng,
                    dynamics_cfg,
                    fav_merchants_idx=ctx.merchant.fav_merchants_idx,
                    fav_k=ctx.merchant.fav_k,
                    contacts=ctx.social.contacts,
                    degree=ctx.social.degree,
                    merch_cdf=ctx.merchant.merch_cdf,
                    total_merchants=len(ctx.merchant.merchants.ids),
                    n_people=total_people,
                )

            day = build_day(self.request, day_index)
            seasonal_mult = monthly_multiplier(day.start.month, seasonal_cfg)

            base_idx = advance_book_through(
                screen_book,
                base_txns,
                base_idx,
                day.start,
                inclusive=False,
            )

            daily_multipliers = advance_all_daily(
                dynamics,
                self.request.rng,
                dynamics_cfg,
                paydays_by_person=ctx.paydays_by_person,
                day_index=day_index,
                person_ids=ctx.population.persons,
                persona_objects=ctx.population.persona_objects,
                fallback_persona=_FALLBACK_PERSONA,
            )

            for prepared in prepared_spenders:
                person_idx = prepared.person_idx
                spender = prepared.spender

                if day_index in prepared.paydays:
                    days_since_payday[person_idx] = 0
                else:
                    days_since_payday[person_idx] += 1

                if screen_book is None:
                    available_cash = prepared.initial_cash
                else:
                    available_cash = screen_book.available_to_spend(
                        spender.deposit_acct
                    )

                liquidity_mult = _liquidity_multiplier(
                    self.request,
                    days_since_payday=days_since_payday[person_idx],
                    paycheck_sensitivity=prepared.paycheck_sensitivity,
                    available_cash=available_cash,
                    baseline_cash=prepared.baseline_cash,
                    fixed_monthly_burden=prepared.fixed_burden,
                )

                combined_mult = daily_multipliers[person_idx] * seasonal_mult

                remaining_person_days = max(
                    1, total_person_days - processed_person_days
                )
                remaining_target_txns = max(0.0, target_total_txns - float(len(txns)))
                target_realized_per_day = remaining_target_txns / float(
                    remaining_person_days
                )

                latent_base_per_day = _latent_base_rate_for_target(
                    spender,
                    day=day.start,
                    day_shock=day.shock,
                    target_realized_per_day=target_realized_per_day,
                    dynamics_multiplier=combined_mult,
                    liquidity_multiplier=liquidity_mult,
                )

                txn_count = sample_txn_count(
                    self.request,
                    spender,
                    day,
                    latent_base_per_day,
                    person_limit,
                    dynamics_multiplier=combined_mult,
                    liquidity_multiplier=liquidity_mult,
                )
                processed_person_days += 1

                if txn_count <= 0:
                    continue

                explore_p = calculate_explore_p(self.request, spender, day)
                explore_floor = float(self.request.params.liquidity_explore_floor)
                explore_p *= max(
                    explore_floor,
                    max(0.0, min(1.0, liquidity_mult**3)),
                )

                accepted = 0
                attempt_budget = max(txn_count, txn_count * 4)

                while accepted < txn_count and attempt_budget > 0:
                    attempt_budget -= 1

                    offsets: I32 = np.asarray(
                        sample_offsets(
                            self.request.rng,
                            spender.persona.timing_profile,
                            1,
                            ctx.population.timing,
                        ),
                        dtype=np.int32,
                    )

                    offset_sec = as_int(cast(int | np.integer, offsets[0]))
                    event = Event(
                        person_idx=person_idx,
                        spender=spender,
                        ts=day.start + timedelta(seconds=offset_sec),
                        txf=txf,
                        explore_p=explore_p,
                    )

                    channel_idx = cdf_pick(channel_cdf, self.request.rng.float())
                    txn = route_channel_txn(
                        channel_idx,
                        self.request,
                        event,
                        prefer_billers_p,
                    )

                    if txn is None:
                        continue

                    if screen_book is not None:
                        decision = screen_book.try_transfer_with_reason(
                            txn.source,
                            txn.target,
                            txn.amount,
                            channel=txn.channel,
                            timestamp=txn.timestamp,
                        )
                        if not decision.accepted:
                            continue

                    txns.append(txn)
                    accepted += 1

        return txns


def generate(request: GenerateRequest) -> list[Transaction]:
    return _Generator(request).run()
