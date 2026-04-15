"""
Vectorized paycheck-boost state for all people.
"""

from dataclasses import dataclass

import numpy as np

from common.math import F64, I32
from math_models.paycheck_cycle import PaycheckCycleConfig

from .person import PersonDynamics


@dataclass(slots=True)
class Paycheck:
    """Vectorized paycheck boost state arrays."""

    boost: F64
    decay: F64
    days_left: I32

    @classmethod
    def from_persons(cls, dynamics: list[PersonDynamics]) -> Paycheck:
        n = len(dynamics)
        boost = np.empty(n, dtype=np.float64)
        decay = np.empty(n, dtype=np.float64)
        days_left = np.empty(n, dtype=np.int32)
        for i, d in enumerate(dynamics):
            boost[i] = d.paycheck_boost.boost
            decay[i] = d.paycheck_boost.daily_decay
            days_left[i] = d.paycheck_boost.days_left
        return cls(boost=boost, decay=decay, days_left=days_left)

    def trigger(
        self,
        cfg: PaycheckCycleConfig,
        is_payday_mask: np.ndarray,
        sensitivities: F64,
    ) -> None:
        """Activate or refresh boost for people who received a paycheck today."""
        if not cfg.enabled or not np.any(is_payday_mask):
            return

        max_boost = float(cfg.max_residual_boost)
        active_days = float(cfg.active_days)
        payday_idx = np.flatnonzero(is_payday_mask)

        new_boosts = np.clip(sensitivities[payday_idx], 0.0, 1.0) * max_boost
        self.boost[payday_idx] = np.maximum(self.boost[payday_idx], new_boosts)
        self.decay[payday_idx] = self.boost[payday_idx] / active_days
        self.days_left[payday_idx] = int(cfg.active_days)

    def advance(self) -> F64:
        """
        Decay all active boosts by one day.

        Returns the paycheck multiplier array for today.
        """
        n = len(self.boost)
        multipliers = np.ones(n, dtype=np.float64)

        active = self.days_left > 0
        if not np.any(active):
            return multipliers

        multipliers[active] = 1.0 + self.boost[active]

        self.days_left[active] -= 1
        self.boost[active] = np.maximum(0.0, self.boost[active] - self.decay[active])

        finished = (self.days_left <= 0) & active
        self.boost[finished] = 0.0
        self.decay[finished] = 0.0
        self.days_left[finished] = 0

        return multipliers
