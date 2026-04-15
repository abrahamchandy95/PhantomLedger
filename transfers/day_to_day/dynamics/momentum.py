"""
Vectorized momentum (AR(1) spending persistence) for all people.
"""

from dataclasses import dataclass

import numpy as np

from common.math import F64
from math_models.momentum import MomentumConfig

from .person import PersonDynamics


@dataclass(slots=True)
class Momentum:
    """Vectorized AR(1) momentum state."""

    values: F64  # shape (n,)

    @classmethod
    def from_persons(cls, dynamics: list[PersonDynamics]) -> Momentum:
        n = len(dynamics)
        values = np.empty(n, dtype=np.float64)
        for i, d in enumerate(dynamics):
            values[i] = d.momentum.value
        return cls(values=values)

    def advance(self, gen: np.random.Generator, cfg: MomentumConfig) -> F64:
        """
        Advance all momentum values by one day.

        Returns the momentum multiplier array for today.
        """
        phi = float(cfg.phi)
        mean_revert = phi * self.values + (1.0 - phi) * 1.0

        if cfg.noise_sigma > 0.0:
            eps = gen.normal(
                loc=0.0, scale=float(cfg.noise_sigma), size=len(self.values)
            )
            raw = mean_revert + eps
        else:
            raw = mean_revert

        self.values = np.clip(raw, float(cfg.floor), float(cfg.ceiling))
        return self.values.copy()
