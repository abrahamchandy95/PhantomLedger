"""
Composed vectorized dynamics for the entire population.
"""

from dataclasses import dataclass

import numpy as np

from common.math import F64

from .dormancy import Dormancy
from .momentum import Momentum
from .paycheck import Paycheck
from .config import DynamicsConfig
from .person import PersonDynamics


@dataclass(slots=True)
class PopulationDynamics:
    """
    Composes the three vectorized sub-states.

    Each sub-state handles its own arrays and advancement logic.
    This class only coordinates calling order and multiplying results.
    """

    momentum: Momentum
    dormancy: Dormancy
    paycheck: Paycheck

    @classmethod
    def from_persons(cls, dynamics: list[PersonDynamics]) -> PopulationDynamics:
        return cls(
            momentum=Momentum.from_persons(dynamics),
            dormancy=Dormancy.from_persons(dynamics),
            paycheck=Paycheck.from_persons(dynamics),
        )

    def advance_day(
        self,
        gen: np.random.Generator,
        cfg: DynamicsConfig,
        is_payday_mask: np.ndarray,
        sensitivities: F64,
    ) -> F64:
        """
        Advance all people by one day.

        Returns per-person rate multipliers (momentum × dormancy × paycheck).
        """
        self.paycheck.trigger(cfg.paycheck, is_payday_mask, sensitivities)

        momentum_mult = self.momentum.advance(gen, cfg.momentum)
        dormancy_mult = self.dormancy.advance(gen, cfg.dormancy)
        paycheck_mult = self.paycheck.advance()

        return momentum_mult * dormancy_mult * paycheck_mult
