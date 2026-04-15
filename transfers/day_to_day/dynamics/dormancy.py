"""
Vectorized dormancy state machine for all people.
"""

from dataclasses import dataclass

import numpy as np
import numpy.typing as npt

from common.math import Bool, F64, I32
from math_models.dormancy import DormancyConfig, Phase

from .person import PersonDynamics


@dataclass(slots=True)
class Dormancy:
    """Vectorized dormancy state arrays."""

    phase: npt.NDArray[np.int8]
    remaining: I32
    wake_total: I32

    @classmethod
    def from_persons(cls, dynamics: list[PersonDynamics]) -> Dormancy:
        n = len(dynamics)
        phase = np.empty(n, dtype=np.int8)
        remaining = np.empty(n, dtype=np.int32)
        wake_total = np.empty(n, dtype=np.int32)
        for i, d in enumerate(dynamics):
            phase[i] = int(d.dormancy.phase)
            remaining[i] = d.dormancy.remaining
            wake_total[i] = d.dormancy.wake_total
        return cls(phase=phase, remaining=remaining, wake_total=wake_total)

    def advance(self, gen: np.random.Generator, cfg: DormancyConfig) -> F64:
        """
        Advance all dormancy states by one day.

        Returns the dormancy multiplier array for today.
        """
        n = len(self.phase)
        dormant_rate = float(cfg.dormant_rate)
        multipliers = np.ones(n, dtype=np.float64)

        is_dormant: Bool = np.asarray(self.phase == int(Phase.DORMANT), dtype=np.bool_)
        is_waking: Bool = np.asarray(self.phase == int(Phase.WAKING), dtype=np.bool_)
        is_active: Bool = np.asarray(self.phase == int(Phase.ACTIVE), dtype=np.bool_)

        multipliers[is_dormant] = dormant_rate

        if np.any(is_waking):
            wt = self.wake_total[is_waking].astype(np.float64)
            rem = self.remaining[is_waking].astype(np.float64)
            safe_wt = np.where(wt > 0, wt, 1.0)
            progress = np.clip(1.0 - rem / safe_wt, 0.0, 1.0)
            multipliers[is_waking] = dormant_rate + progress * (1.0 - dormant_rate)

        # --- Phase transitions ---
        self._transition_active(gen, cfg, is_active, n)
        self._transition_dormant(gen, cfg, is_dormant)
        self._transition_waking(is_waking)

        return multipliers

    def _transition_active(
        self,
        gen: np.random.Generator,
        cfg: DormancyConfig,
        is_active: np.ndarray,
        n: int,
    ) -> None:
        """ACTIVE → DORMANT with probability enter_p."""
        if not np.any(is_active):
            return

        enters = is_active & (gen.random(n) < float(cfg.enter_p))
        if not np.any(enters):
            return

        count = int(np.count_nonzero(enters))
        self.phase[enters] = 1
        self.remaining[enters] = gen.integers(
            int(cfg.duration_min),
            int(cfg.duration_max) + 1,
            size=count,
        ).astype(np.int32)

    def _transition_dormant(
        self,
        gen: np.random.Generator,
        cfg: DormancyConfig,
        is_dormant: np.ndarray,
    ) -> None:
        """DORMANT countdown → WAKING when remaining reaches 0."""
        if not np.any(is_dormant):
            return

        self.remaining[is_dormant] -= 1
        to_wake = is_dormant & (self.remaining <= 0)
        if not np.any(to_wake):
            return

        count = int(np.count_nonzero(to_wake))
        self.phase[to_wake] = 2
        wt_new = gen.integers(
            int(cfg.wake_days_min),
            int(cfg.wake_days_max) + 1,
            size=count,
        ).astype(np.int32)
        self.wake_total[to_wake] = wt_new
        self.remaining[to_wake] = wt_new

    def _transition_waking(self, is_waking: np.ndarray) -> None:
        """WAKING countdown → ACTIVE when remaining reaches 0."""
        if not np.any(is_waking):
            return

        self.remaining[is_waking] -= 1
        to_active = is_waking & (self.remaining <= 0)
        if not np.any(to_active):
            return

        self.phase[to_active] = 0
        self.remaining[to_active] = 0
        self.wake_total[to_active] = 0
