from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

import numpy as np

import entities.models as models
from common.math import Scalar, as_float, as_int
from common.persona_names import SALARIED
from entities.personas import PERSONAS
from math_models.counts import DEFAULT_RATES, gamma_poisson, weekday_multiplier
from transfers.factory import TransactionFactory

from .engine import GenerateRequest


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


def build_day(request: GenerateRequest, index: int) -> Day:
    start = request.start_date + timedelta(days=index)
    is_weekend = start.weekday() >= 5

    shape = float(request.events.day_multiplier_shape)
    shock = float(request.rng.gen.gamma(shape=shape, scale=(1.0 / shape)))

    return Day(
        index=index,
        start=start,
        is_weekend=is_weekend,
        shock=shock,
    )


def build_spender(request: GenerateRequest, index: int) -> Spender | None:
    pop = request.ctx.population
    merch = request.ctx.merchant

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


def sample_txn_count(
    request: GenerateRequest,
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

    dynamics_multiplier captures behavior state.
    liquidity_multiplier captures affordability pressure and should have
    a stronger-than-linear effect so that late-cycle stress suppresses
    attempts before replay has to reject them.
    """
    liq = max(0.0, min(1.25, float(liquidity_multiplier)))

    rate = (
        base_rate
        * float(spender.persona.rate_multiplier)
        * float(weekday_multiplier(day.start, DEFAULT_RATES))
        * day.shock
        * dynamics_multiplier
        * (liq * liq)
    )

    # Hard suppression for highly stressed days.
    if liq < 0.25:
        return 0

    # Probabilistic suppression in the stress band.
    if liq < 0.45 and request.rng.float() > liq:
        return 0

    count = gamma_poisson(
        request.rng,
        base_rate=rate,
        rates=DEFAULT_RATES,
    )

    if count <= 0:
        return 0

    if liq < 0.45:
        count = min(count, 1)
    elif liq < 0.65:
        count = min(count, 2)

    if limit is not None:
        count = min(count, max(0, limit))

    return count


def calculate_explore_p(
    request: GenerateRequest,
    spender: Spender,
    day: Day,
) -> float:
    base_p = float(request.merchants_cfg.explore_p)
    params = request.params

    explore_p = base_p * (0.25 + 0.75 * spender.explore_prop)

    if day.is_weekend:
        explore_p *= float(params.weekend_boost)

    in_burst_window = (
        spender.burst_start >= 0
        and spender.burst_len > 0
        and spender.burst_start <= day.index < (spender.burst_start + spender.burst_len)
    )

    if in_burst_window:
        explore_p *= float(params.burst_boost)

    return min(0.50, max(0.0, explore_p))
