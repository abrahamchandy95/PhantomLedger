"""
Account dormancy and reactivation model.

Real bank accounts go through dormant periods — weeks or months of
near-zero activity triggered by life events (vacation, hospital stay,
switching to a different account, seasonal work patterns). This is
critical for mule detection because:

1. Legitimate dormancy exists: ~35% of bank accounts show at least
   one period of 14+ consecutive days with zero transactions per year
   (estimated from Fed Payments Study transaction frequency data).

2. Mule dormancy is structurally different: accounts are opened or
   recruited, left dormant to establish legitimacy, then activated
   with a sudden high-volume burst (LexisNexis 2025, "Money Mule
   Detection Strategies"). Recruited mules show: dormant → tester
   payments → spike. Complicit mules show: progressive buildup.

3. Legitimate reactivation is gradual: when a real person returns
   from vacation, spending ramps back to baseline over days. Unit21
   (2026) found reactivation transactions average 17× the rule
   threshold, but legitimate reactivation shows a gradual ramp
   while fraudulent reactivation is immediate and high-volume.

We model dormancy as a three-state machine per person:
  ACTIVE  → can go dormant with probability p_enter per day
  DORMANT → stays dormant for a sampled duration, then transitions
  WAKING  → ramp-up period (2-5 days) where rate multiplier
            linearly interpolates from dormant level to baseline

This ensures ML models trained on this data must learn to
distinguish "normal quiet period → gradual ramp" from
"mule dormancy → sudden burst."
"""

from dataclasses import dataclass
from enum import IntEnum

from common.random import Rng
from common.validate import between, ge


class Phase(IntEnum):
    ACTIVE = 0
    DORMANT = 1
    WAKING = 2


@dataclass(frozen=True, slots=True)
class DormancyConfig:
    """Controls the dormancy state machine."""

    # Daily probability of entering dormancy (while ACTIVE).
    # Calibrated so ~30-40% of accounts experience at least one
    # dormant spell per year: 1 - (1 - p)^365 ≈ 0.35 → p ≈ 0.0012
    enter_p: float = 0.0012

    # Dormant spell duration bounds (days).
    # Vacation: 7-21 days. Hospital: 5-30. Seasonal: 30-90.
    # We sample uniformly in [min, max] for simplicity.
    duration_min: int = 7
    duration_max: int = 45

    # Wake-up ramp duration bounds (days).
    # How many days to linearly ramp from dormant → full activity.
    wake_days_min: int = 2
    wake_days_max: int = 5

    # Rate multiplier during dormancy. Not strictly zero because
    # some automatic payments (subscriptions, bills) still fire.
    dormant_rate: float = 0.05

    def __post_init__(self) -> None:
        between("enter_p", self.enter_p, 0.0, 0.1)
        ge("duration_min", self.duration_min, 1)
        ge("duration_max", self.duration_max, self.duration_min)
        ge("wake_days_min", self.wake_days_min, 1)
        ge("wake_days_max", self.wake_days_max, self.wake_days_min)
        between("dormant_rate", self.dormant_rate, 0.0, 1.0)


DEFAULT_DORMANCY_CONFIG = DormancyConfig()


@dataclass(slots=True)
class DormancyState:
    """Mutable per-person dormancy tracker."""

    phase: Phase = Phase.ACTIVE

    # Countdown: days remaining in current DORMANT or WAKING phase.
    remaining: int = 0

    # Total wake duration (for interpolation).
    wake_total: int = 0

    def rate_multiplier(self, cfg: DormancyConfig) -> float:
        """Returns the spending rate multiplier for the current phase."""
        if self.phase == Phase.ACTIVE:
            return 1.0
        if self.phase == Phase.DORMANT:
            return float(cfg.dormant_rate)

        # WAKING: linear ramp from dormant_rate to 1.0
        if self.wake_total <= 0:
            return 1.0
        progress = 1.0 - (float(self.remaining) / float(self.wake_total))
        progress = max(0.0, min(1.0, progress))
        return float(cfg.dormant_rate) + progress * (1.0 - float(cfg.dormant_rate))

    def advance(self, rng: Rng, cfg: DormancyConfig) -> float:
        """
        Advance the state machine by one day.
        Returns the rate multiplier for today.
        """
        multiplier = self.rate_multiplier(cfg)

        if self.phase == Phase.ACTIVE:
            if rng.coin(float(cfg.enter_p)):
                self.phase = Phase.DORMANT
                self.remaining = rng.int(
                    int(cfg.duration_min), int(cfg.duration_max) + 1
                )
            return multiplier

        if self.phase == Phase.DORMANT:
            self.remaining -= 1
            if self.remaining <= 0:
                self.phase = Phase.WAKING
                self.wake_total = rng.int(
                    int(cfg.wake_days_min), int(cfg.wake_days_max) + 1
                )
                self.remaining = self.wake_total
            return multiplier

        # Phase.WAKING
        self.remaining -= 1
        if self.remaining <= 0:
            self.phase = Phase.ACTIVE
            self.remaining = 0
            self.wake_total = 0
        return multiplier
