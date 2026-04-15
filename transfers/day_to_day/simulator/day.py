from collections.abc import Sequence
from dataclasses import dataclass, field
from datetime import timedelta
from typing import Protocol

from common.math import F64, cdf_pick
from common.transactions import Transaction
from math_models.seasonal import monthly_multiplier
from math_models.timing import sample_offset
from transfers.screening import advance_book_through

from ..actors import (
    Event,
    build_day,
    calculate_explore_p,
    sample_txn_count,
)
from ..dynamics import (
    ContactsView,
    DayContext,
    FavoritesView,
    PopulationDynamics,
    advance_all_batch,
    advance_all_scalar,
    evolve_all,
)
from ..environment import Market, Parameters, TransactionEngine
from .calendar import is_month_boundary
from .liquidity import LiquiditySnapshot, liquidity_multiplier
from .routing import RoutePolicy, route_txn
from .spenders import PreparedSpender
from .targets import base_rate_for_target


class DayRunner(Protocol):
    market: Market
    engine: TransactionEngine
    params: Parameters


@dataclass(frozen=True, slots=True)
class RunPlan:
    prepared_spenders: list[PreparedSpender]
    target_total_txns: float
    total_person_days: int
    base_txns: Sequence[Transaction]
    sensitivities: F64
    batch: PopulationDynamics | None
    favorites: FavoritesView
    contacts: ContactsView
    route_policy: RoutePolicy
    person_limit: int | None
    channel_cdf: F64


@dataclass(slots=True)
class RunState:
    days_since_payday: list[int]
    txns: list[Transaction] = field(default_factory=list)
    base_idx: int = 0
    processed_person_days: int = 0


def run_day(
    simulator: DayRunner,
    plan: RunPlan,
    state: RunState,
    day_index: int,
) -> None:
    market = simulator.market
    engine = simulator.engine
    params = simulator.params

    rng = engine.rng
    txf = engine.txf
    clearing_house = engine.clearing_house

    if is_month_boundary(day_index, market.start_date):
        evolve_all(
            rng,
            params.dynamics.evolution,
            plan.favorites,
            plan.contacts,
        )

    day = build_day(
        market.start_date,
        float(market.events.day_multiplier_shape),
        rng,
        day_index,
    )
    seasonal_mult = monthly_multiplier(day.start.month, params.seasonal)

    state.base_idx = advance_book_through(
        clearing_house,
        plan.base_txns,
        state.base_idx,
        day.start,
        inclusive=False,
    )

    day_ctx = DayContext(
        day_index=day_index,
        paydays_by_person=market.paydays,
        sensitivities=plan.sensitivities,
    )

    if plan.batch is None:
        daily_multipliers = advance_all_scalar(
            market.person_states,
            rng,
            params.dynamics,
            day_ctx,
        )
    else:
        daily_multipliers = advance_all_batch(
            plan.batch,
            rng.gen,
            params.dynamics,
            day_ctx,
        )

    for prepared in plan.prepared_spenders:
        person_idx = prepared.person_idx
        spender = prepared.spender

        if day_index in prepared.paydays:
            state.days_since_payday[person_idx] = 0
        else:
            state.days_since_payday[person_idx] += 1

        if clearing_house is None:
            available_cash = prepared.initial_cash
        else:
            available_cash = clearing_house.available_to_spend(spender.deposit_acct)

        liquidity_mult = liquidity_multiplier(
            params.liquidity_constraints,
            LiquiditySnapshot(
                days_since_payday=state.days_since_payday[person_idx],
                paycheck_sensitivity=prepared.paycheck_sensitivity,
                available_cash=available_cash,
                baseline_cash=prepared.baseline_cash,
                fixed_monthly_burden=prepared.fixed_burden,
            ),
        )

        combined_mult = float(daily_multipliers[person_idx]) * float(seasonal_mult)

        remaining_person_days = max(
            1,
            plan.total_person_days - state.processed_person_days,
        )
        remaining_target_txns = max(
            0.0, plan.target_total_txns - float(len(state.txns))
        )
        target_realized_per_day = remaining_target_txns / float(remaining_person_days)

        latent_base_per_day = base_rate_for_target(
            spender,
            day.shock,
            day.start,
            target_realized_per_day,
            combined_mult,
            liquidity_mult,
        )

        txn_count = sample_txn_count(
            rng,
            spender,
            day,
            latent_base_per_day,
            plan.person_limit,
            dynamics_multiplier=combined_mult,
            liquidity_multiplier=liquidity_mult,
        )
        state.processed_person_days += 1

        if txn_count <= 0:
            continue

        explore_p = calculate_explore_p(
            float(market.merchants_cfg.explore_p),
            params.exploration_habits,
            params.burst_behavior,
            spender,
            day,
        )
        explore_p *= max(
            float(params.liquidity_constraints.exploration_floor),
            max(0.0, min(1.0, liquidity_mult**3)),
        )

        accepted = 0
        attempt_budget = max(txn_count, txn_count * 4)

        while accepted < txn_count and attempt_budget > 0:
            attempt_budget -= 1

            offset_sec = sample_offset(
                rng,
                spender.persona.timing_profile,
                market.population.timing,
            )
            event = Event(
                person_idx=person_idx,
                spender=spender,
                ts=day.start + timedelta(seconds=offset_sec),
                txf=txf,
                explore_p=explore_p,
            )

            channel_idx = cdf_pick(plan.channel_cdf, rng.float())
            txn = route_txn(
                rng,
                market,
                plan.route_policy,
                channel_idx,
                event,
            )
            if txn is None:
                continue

            if clearing_house is not None:
                decision = clearing_house.try_transfer_with_reason(
                    txn.source,
                    txn.target,
                    txn.amount,
                    channel=txn.channel,
                    timestamp=txn.timestamp,
                )
                if not decision.accepted:
                    continue

            state.txns.append(txn)
            accepted += 1
