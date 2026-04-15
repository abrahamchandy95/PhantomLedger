"""
Scalar per-person behavioral state.

Composes momentum, dormancy, and paycheck-boost into one advanceable
unit. Used for small populations or as the reference implementation.
"""

from dataclasses import dataclass, field

from common.random import Rng
from math_models.momentum import MomentumState
from math_models.dormancy import DormancyState
from math_models.paycheck_cycle import PaycheckBoostState

from .config import DynamicsConfig


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
