from dataclasses import dataclass
from datetime import datetime
from typing import cast

import numpy as np

from common.math import NumScalar, as_float, as_int
from entities.personas import PERSONAS, PersonaSpec
from math_models.counts import (
    DEFAULT_COUNT_MODELS,
    gamma_poisson_out_count,
    weekday_multiplier,
)
from transfers.txns import TxnFactory

from .models import DayToDayContext, DayToDayGenerationRequest


@dataclass(frozen=True, slots=True)
class PersonState:
    person_id: str
    deposit_acct: str
    persona_name: str
    persona: PersonaSpec
    favorite_k: int
    biller_k: int
    explore_propensity: float
    burst_start: int
    burst_len: int


@dataclass(frozen=True, slots=True)
class DayFrame:
    day_index: int
    day_start: datetime
    is_weekend: bool
    day_shock: float


@dataclass(frozen=True, slots=True)
class EventFrame:
    person_idx: int
    person_state: PersonState
    txn_ts: datetime
    txf: TxnFactory
    explore_p: float


def build_day_frame(
    request: DayToDayGenerationRequest,
    day_index: int,
) -> DayFrame:
    from datetime import timedelta

    day_start = request.start_date + timedelta(days=day_index)
    is_weekend = day_start.weekday() >= 5

    gamma_shape = float(request.events.day_multiplier_gamma_shape)
    day_shock = float(
        request.rng.gen.gamma(shape=gamma_shape, scale=(1.0 / gamma_shape))
    )

    return DayFrame(
        day_index=day_index,
        day_start=day_start,
        is_weekend=is_weekend,
        day_shock=day_shock,
    )


def build_person_state(
    request: DayToDayGenerationRequest,
    person_idx: int,
) -> PersonState | None:
    ctx: DayToDayContext = request.ctx

    person_id = ctx.persons[person_idx]
    deposit_acct = ctx.primary_acct_for_person.get(person_id)
    if deposit_acct is None:
        return None

    persona_name = ctx.persona_for_person.get(person_id, "salaried")
    persona = PERSONAS.get(persona_name, PERSONAS["salaried"])

    favorite_k = as_int(cast(int | np.integer, ctx.fav_k[person_idx]))
    biller_k = as_int(cast(int | np.integer, ctx.bill_k[person_idx]))
    explore_propensity = as_float(cast(NumScalar, ctx.explore_propensity[person_idx]))
    burst_start = as_int(cast(int | np.integer, ctx.burst_start_day[person_idx]))
    burst_len = as_int(cast(int | np.integer, ctx.burst_len[person_idx]))

    return PersonState(
        person_id=person_id,
        deposit_acct=deposit_acct,
        persona_name=persona_name,
        persona=persona,
        favorite_k=favorite_k,
        biller_k=biller_k,
        explore_propensity=explore_propensity,
        burst_start=burst_start,
        burst_len=burst_len,
    )


def compute_outgoing_count(
    request: DayToDayGenerationRequest,
    person_state: PersonState,
    day_frame: DayFrame,
    base_per_day: float,
    remaining_today: int | None,
) -> int:
    rate = (
        base_per_day
        * float(person_state.persona.rate_mult)
        * float(weekday_multiplier(day_frame.day_start, DEFAULT_COUNT_MODELS))
        * day_frame.day_shock
    )

    n_out = gamma_poisson_out_count(
        request.rng,
        base_rate=rate,
        models=DEFAULT_COUNT_MODELS,
    )
    if n_out <= 0:
        return 0

    if remaining_today is not None:
        n_out = min(n_out, max(0, remaining_today))
    return n_out


def compute_explore_probability(
    request: DayToDayGenerationRequest,
    person_state: PersonState,
    day_frame: DayFrame,
) -> float:
    explore_base = float(request.merchants_cfg.explore_p)
    policy = request.policy

    explore_p = explore_base * (0.25 + 0.75 * person_state.explore_propensity)

    if day_frame.is_weekend:
        explore_p *= float(policy.weekend_explore_multiplier)

    if (
        person_state.burst_start >= 0
        and person_state.burst_len > 0
        and (
            person_state.burst_start
            <= day_frame.day_index
            < person_state.burst_start + person_state.burst_len
        )
    ):
        explore_p *= float(policy.burst_explore_multiplier)

    return min(0.50, max(0.0, explore_p))
