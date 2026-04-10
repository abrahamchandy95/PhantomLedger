"""
Per-person dynamic behavioral state for the day-to-day engine.

Bundles momentum (AR(1) spending persistence), dormancy (activity
state machine), and monthly counterparty evolution into a single
structure that advances each simulation day.

This is the integration layer between the math_models and the
day-to-day generator. The generator calls advance_day() once per
person per day, and evolve_month() once per person per month boundary.
"""

from dataclasses import dataclass, field

import numpy as np

from common.math import F64, I32, I16
from common.random import Rng
import entities.models as models
from math_models.momentum import MomentumConfig, MomentumState
from math_models.dormancy import DormancyConfig, DormancyState
from math_models.paycheck_cycle import PaycheckBoostState, PaycheckCycleConfig
from math_models.counterparty_evolution import (
    EvolutionConfig,
    evolve_contacts,
    evolve_favorites,
)


@dataclass(frozen=True, slots=True)
class DynamicsConfig:
    """Aggregated configuration for all per-person dynamics."""

    momentum: MomentumConfig = field(default_factory=MomentumConfig)
    dormancy: DormancyConfig = field(default_factory=DormancyConfig)
    paycheck: PaycheckCycleConfig = field(default_factory=PaycheckCycleConfig)
    evolution: EvolutionConfig = field(default_factory=EvolutionConfig)


DEFAULT_DYNAMICS_CONFIG = DynamicsConfig()


@dataclass(slots=True)
class PersonDynamics:
    """Mutable per-person behavioral state."""

    momentum: MomentumState = field(default_factory=MomentumState)
    dormancy: DormancyState = field(default_factory=DormancyState)
    paycheck_boost: PaycheckBoostState = field(default_factory=PaycheckBoostState)

    def advance_day(
        self,
        rng: Rng,
        cfg: DynamicsConfig,
        *,
        is_payday: bool = False,
        paycheck_sensitivity: float = 0.0,
    ) -> float:
        """
        Advance all daily dynamics by one step.

        Returns the combined rate multiplier for today.
        The two multipliers are independent and compose multiplicatively:
          - momentum: captures week-to-week spending persistence
          - dormancy: captures longer inactive spells and ramp-ups
          - payday_boost: short post-income lift in discretionary spending
        """
        if is_payday:
            boost = cfg.paycheck.boost_for_sensitivity(paycheck_sensitivity)
            self.paycheck_boost.trigger(boost, cfg.paycheck)

        momentum_mult = self.momentum.advance(rng, cfg.momentum)
        dormancy_mult = self.dormancy.advance(rng, cfg.dormancy)
        payday_mult = self.paycheck_boost.advance()
        return momentum_mult * dormancy_mult * payday_mult


def initialize_dynamics(n_people: int) -> list[PersonDynamics]:
    """Create fresh dynamic state for every person in the simulation."""
    return [PersonDynamics() for _ in range(n_people)]


def advance_all_daily(
    dynamics: list[PersonDynamics],
    rng: Rng,
    cfg: DynamicsConfig,
    *,
    paydays_by_person: tuple[frozenset[int], ...],
    day_index: int,
    person_ids: list[str],
    persona_objects: dict[str, models.Persona],
    fallback_persona: models.Persona,
) -> list[float]:
    """
    Advance every person's dynamics by one day.

    Returns a list of per-person rate multipliers for today.
    This must run for every person regardless of whether they
    can generate transactions today.
    """
    daily_multipliers: list[float] = []

    for person_idx, dyn in enumerate(dynamics):
        is_payday = day_index in paydays_by_person[person_idx]

        person_id = person_ids[person_idx]
        persona = persona_objects.get(person_id, fallback_persona)
        sensitivity = float(persona.paycheck_sensitivity)

        mult = dyn.advance_day(
            rng,
            cfg,
            is_payday=is_payday,
            paycheck_sensitivity=sensitivity,
        )
        daily_multipliers.append(mult)

    return daily_multipliers


def evolve_all_monthly(
    rng: Rng,
    cfg: DynamicsConfig,
    *,
    fav_merchants_idx: I32,
    fav_k: I16,
    contacts: I32,
    degree: int,
    merch_cdf: F64,
    total_merchants: int,
    n_people: int,
) -> None:
    """
    Evolve counterparty sets for all people. Called at month boundaries.

    Mutates fav_merchants_idx, fav_k, and contacts arrays in place
    to avoid reallocation.
    """
    from typing import cast

    from common.math import as_int

    evo_cfg = cfg.evolution
    max_cols = as_int(cast(int | np.integer, fav_merchants_idx.shape[1]))

    for i in range(n_people):
        # --- Merchant favorites ---
        current_k = as_int(cast(int | np.integer, fav_k[i]))
        current_favs = [
            as_int(cast(int | np.integer, fav_merchants_idx[i, j]))
            for j in range(current_k)
        ]

        new_favs = evolve_favorites(
            rng,
            evo_cfg,
            current_favs,
            merch_cdf,
            total_merchants,
        )

        # Write back into the fixed-width array
        new_k = min(len(new_favs), max_cols)
        fav_k[i] = np.int16(new_k)

        if new_k > 0:
            fav_merchants_idx[i, :] = np.int32(new_favs[0])
            for j in range(new_k):
                fav_merchants_idx[i, j] = np.int32(new_favs[j])

        # --- P2P contacts ---
        row: I32 = np.asarray(contacts[i], dtype=np.int32)
        contacts[i] = evolve_contacts(
            rng,
            evo_cfg,
            row,
            degree,
            i,
            n_people,
        )
