from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

import numpy as np

from common.math import I32, as_int, cdf_pick
from common.persona_names import SALARIED
from common.transactions import Transaction
from entities.personas import PERSONAS
from math_models.seasonal import monthly_multiplier
from math_models.timing import sample_offsets
from transfers.factory import TransactionFactory

from .channels import build_channel_cdf, route_channel_txn
from .dynamics import advance_all_daily, evolve_all_monthly
from .engine import GenerateRequest
from .state import (
    Event,
    build_day,
    build_spender,
    calculate_explore_p,
    sample_txn_count,
)

_FALLBACK_PERSONA = PERSONAS[SALARIED]


def _is_month_boundary(
    day_index: int,
    prev_day_index: int,
    start_date: datetime,
) -> bool:
    """Check if we've crossed into a new calendar month."""
    if day_index == 0:
        return False
    prev = start_date + timedelta(days=prev_day_index)
    curr = start_date + timedelta(days=day_index)
    return curr.month != prev.month or curr.year != prev.year


@dataclass(slots=True)
class _Generator:
    """Orchestrates the daily simulation loop for all spenders."""

    request: GenerateRequest

    def run(self) -> list[Transaction]:
        if self.request.days <= 0:
            return []

        txf = TransactionFactory(rng=self.request.rng, infra=self.request.infra)
        channel_cdf = build_channel_cdf(self.request)

        base_per_day = float(self.request.merchants_cfg.txns_per_month) / 30.0
        prefer_billers_p = float(self.request.events.prefer_billers_p)
        daily_limit = int(self.request.events.max_per_day)

        dynamics_cfg = self.request.params.dynamics
        seasonal_cfg = self.request.params.seasonal
        ctx = self.request.ctx
        dynamics = ctx.person_dynamics
        num_spenders = len(ctx.population.persons)

        txns: list[Transaction] = []

        for day_index in range(self.request.days):
            # --- Monthly counterparty evolution at month boundaries ---
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
                    n_people=num_spenders,
                )

            day = build_day(self.request, day_index)

            # Seasonal multiplier: applies uniformly to all spenders today,
            # derived from the calendar month of the simulation day.
            # This is a population-wide effect (everyone shops more in
            # December) so we compute it once per day, not per person.
            seasonal_mult = monthly_multiplier(day.start.month, seasonal_cfg)
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

            txns_today = 0

            for person_idx in range(num_spenders):
                if 0 < daily_limit <= txns_today:
                    break

                spender = build_spender(self.request, person_idx)
                if spender is None:
                    continue

                remaining_today = (
                    None if daily_limit <= 0 else max(0, daily_limit - txns_today)
                )

                combined_mult = daily_multipliers[person_idx] * seasonal_mult

                txn_count = sample_txn_count(
                    self.request,
                    spender,
                    day,
                    base_per_day,
                    remaining_today,
                    dynamics_multiplier=combined_mult,
                )
            txns_today = 0

            for person_idx in range(num_spenders):
                if 0 < daily_limit <= txns_today:
                    break

                spender = build_spender(self.request, person_idx)
                if spender is None:
                    continue

                is_payday = day_index in ctx.paydays_by_person[person_idx]
                daily_multiplier = dynamics[person_idx].advance_day(
                    self.request.rng,
                    dynamics_cfg,
                    is_payday=is_payday,
                    paycheck_sensitivity=float(spender.persona.paycheck_sensitivity),
                )
                remaining_today = (
                    None if daily_limit <= 0 else max(0, daily_limit - txns_today)
                )

                # Combine per-person dynamics with population seasonal effect.
                combined_mult = daily_multiplier * seasonal_mult

                txn_count = sample_txn_count(
                    self.request,
                    spender,
                    day,
                    base_per_day,
                    remaining_today,
                    dynamics_multiplier=combined_mult,
                )

                if txn_count <= 0:
                    continue

                offsets: I32 = np.asarray(
                    sample_offsets(
                        self.request.rng,
                        spender.persona.timing_profile,
                        txn_count,
                        ctx.population.timing,
                    ),
                    dtype=np.int32,
                )

                explore_p = calculate_explore_p(self.request, spender, day)

                for i in range(txn_count):
                    offset_sec = as_int(cast(int | np.integer, offsets[i]))
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

                    if txn is not None:
                        txns.append(txn)
                        txns_today += 1

                    if 0 < daily_limit <= txns_today:
                        break

        return txns


def generate(request: GenerateRequest) -> list[Transaction]:
    """Entry point for the day-to-day transaction simulation engine."""
    return _Generator(request).run()
