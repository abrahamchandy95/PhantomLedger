"""
Daily dynamics advancement orchestration.

Single responsibility: advance the population's behavioral state
by one day, supporting both scalar and vectorized paths.
"""

from dataclasses import dataclass

import numpy as np

from common.math import F64
from common.random import Rng

from .population import PopulationDynamics
from .config import DynamicsConfig
from .person import PersonDynamics


@dataclass(frozen=True, slots=True)
class DayContext:
    """Per-day inputs needed by dynamics advancement."""

    day_index: int
    paydays_by_person: tuple[frozenset[int], ...]
    sensitivities: F64  # pre-computed paycheck_sensitivity per person


def build_payday_mask(ctx: DayContext) -> np.ndarray:
    """Build a boolean mask of who receives a paycheck today."""
    n = len(ctx.paydays_by_person)
    return np.array(
        [ctx.day_index in ctx.paydays_by_person[i] for i in range(n)],
        dtype=np.bool_,
    )


def advance_all_scalar(
    dynamics: list[PersonDynamics],
    rng: Rng,
    cfg: DynamicsConfig,
    ctx: DayContext,
) -> list[float]:
    """
    Advance every person's dynamics by one day using the scalar path.

    Returns a list of per-person rate multipliers for today.
    """
    day_index = ctx.day_index
    paydays = ctx.paydays_by_person
    sensitivities = ctx.sensitivities

    multipliers: list[float] = []

    for person_idx, dyn in enumerate(dynamics):
        is_payday = day_index in paydays[person_idx]
        sensitivity = float(sensitivities.item(person_idx))

        mult = dyn.advance_day(
            rng,
            cfg,
            is_payday=is_payday,
            paycheck_sensitivity=sensitivity,
        )
        multipliers.append(mult)

    return multipliers


def advance_all_batch(
    batch: PopulationDynamics,
    gen: np.random.Generator,
    cfg: DynamicsConfig,
    ctx: DayContext,
) -> F64:
    """
    Advance every person's dynamics by one day using the vectorized path.

    Returns numpy array of per-person rate multipliers.
    """
    is_payday_mask = build_payday_mask(ctx)

    return batch.advance_day(
        gen,
        cfg,
        is_payday_mask,
        ctx.sensitivities,
    )
