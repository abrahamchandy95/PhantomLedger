from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

import numpy as np

import entities.models as models
from common.math import Scalar, as_float, as_int
from common.persona_names import SALARIED
from common.random import Rng
from entities.personas import PERSONAS
from math_models.counts import DEFAULT_RATES, weekday_multiplier
from transfers.factory import TransactionFactory

from .environment import (
    BurstBehavior,
    ExplorationHabits,
    MerchantView,
    PopulationView,
)


_FALLBACK_PERSONA = PERSONAS[SALARIED]


@dataclass(frozen=True, slots=True)
class Spender:
    """Snapshot of a person's financial mapping and behavioral dials."""

    id: str
    deposit_acct: str
    persona_name: str
    persona: models.Persona
    fav_k: int
    bill_k: int
    explore_prop: float
    burst_start: int
    burst_len: int


@dataclass(frozen=True, slots=True)
class Day:
    """The temporal constraints and global modifiers for a single simulation day."""

    index: int
    start: datetime
    is_weekend: bool
    shock: float


@dataclass(frozen=True, slots=True)
class Event:
    """The specific context passed down to evaluate a single transaction event."""

    person_idx: int
    spender: Spender
    ts: datetime
    txf: TransactionFactory
    explore_p: float


def build_day(
    start_date: datetime,
    day_multiplier_shape: float,
    rng: Rng,
    index: int,
) -> Day:
    start = start_date + timedelta(days=index)
    is_weekend = start.weekday() >= 5

    shape = float(day_multiplier_shape)
    shock = float(rng.gen.gamma(shape=shape, scale=(1.0 / shape)))

    return Day(
        index=index,
        start=start,
        is_weekend=is_weekend,
        shock=shock,
    )


def build_spender(
    pop: PopulationView,
    merch: MerchantView,
    index: int,
) -> Spender | None:
    person_id = pop.persons[index]
    deposit_acct = pop.primary_accounts.get(person_id)
    if deposit_acct is None:
        return None

    persona = pop.persona_objects.get(person_id, _FALLBACK_PERSONA)

    return Spender(
        id=person_id,
        deposit_acct=deposit_acct,
        persona_name=persona.name,
        persona=persona,
        fav_k=as_int(cast(int | np.integer, merch.fav_k[index])),
        bill_k=as_int(cast(int | np.integer, merch.bill_k[index])),
        explore_prop=as_float(cast(Scalar, merch.explore_propensity[index])),
        burst_start=as_int(cast(int | np.integer, merch.burst_start_day[index])),
        burst_len=as_int(cast(int | np.integer, merch.burst_len[index])),
    )


def count_liquidity_factor(liquidity_multiplier: float) -> float:
    """
    Soft count-stage liquidity shaping.

    This intentionally never zeroes out an entire person-day. Count
    allocation should stay target-seeking, while true affordability is
    enforced later by the replay/screening ledger.
    """
    liq = max(0.0, min(1.25, float(liquidity_multiplier)))

    if liq <= 1.0:
        softened = 0.50 + (0.50 * liq)
    else:
        softened = liq

    return softened * softened


def sample_txn_count(
    rng: Rng,
    spender: Spender,
    day: Day,
    base_rate: float,
    limit: int | None,
    *,
    dynamics_multiplier: float = 1.0,
    liquidity_multiplier: float = 1.0,
) -> int:
    """
    Sample the number of outbound transactions for one person-day.

    `base_rate` is the latent upstream intensity for this specific
    person-day. The outer generator calibrates it from the configured
    realized monthly target after inverting all count-stage suppressors.

    We then convert the expected realized count into an integer with
    stochastic rounding instead of adding another free Poisson layer.
    Day-level burstiness still comes from `day.shock`.
    """
    dynamic_mult = max(0.0, float(dynamics_multiplier))
    rate = (
        base_rate
        * float(spender.persona.rate_multiplier)
        * float(weekday_multiplier(day.start, DEFAULT_RATES))
        * float(day.shock)
        * dynamic_mult
        * count_liquidity_factor(liquidity_multiplier)
    )

    if rate <= 0.0:
        return 0

    whole = int(rate)
    frac = float(rate) - float(whole)

    count = whole
    if rng.float() < frac:
        count += 1

    if limit is not None:
        count = min(count, max(0, limit))

    return max(0, count)


def calculate_explore_p(
    base_explore_p: float,
    explore_habits: ExplorationHabits,
    burst_behavior: BurstBehavior,
    spender: Spender,
    day: Day,
) -> float:
    explore_p = base_explore_p * (0.25 + 0.75 * spender.explore_prop)

    if day.is_weekend:
        explore_p *= float(explore_habits.weekend_multiplier)

    in_burst_window = (
        spender.burst_start >= 0
        and spender.burst_len > 0
        and spender.burst_start <= day.index < (spender.burst_start + spender.burst_len)
    )

    if in_burst_window:
        explore_p *= float(burst_behavior.multiplier)

    return min(0.50, max(0.0, explore_p))
