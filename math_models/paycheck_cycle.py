"""
Short-lived post-payday boost for discretionary spending.

The day-to-day engine already isolates mostly non-recurring consumer
activity, so this module models the residual payday-cycle effect rather
than the full spike driven by recurring bills.
"""

from dataclasses import dataclass

from common.validate import between, ge


@dataclass(frozen=True, slots=True)
class PaycheckCycleConfig:
    """Controls the size and duration of the payday spending lift."""

    enabled: bool = True

    # Residual discretionary effect ceiling.
    max_residual_boost: float = 0.10

    # Number of elevated days including payday itself.
    active_days: int = 4

    def __post_init__(self) -> None:
        between("max_residual_boost", self.max_residual_boost, 0.0, 1.0)
        ge("active_days", self.active_days, 1)

    def boost_for_sensitivity(self, sensitivity: float) -> float:
        if not self.enabled:
            return 0.0

        clipped = max(0.0, min(1.0, float(sensitivity)))
        return clipped * float(self.max_residual_boost)


@dataclass(slots=True)
class PaycheckBoostState:
    """Tracks an active payday window and decays it linearly."""

    boost: float = 0.0
    daily_decay: float = 0.0
    days_left: int = 0

    def trigger(self, boost: float, cfg: PaycheckCycleConfig) -> None:
        """
        Start or refresh a payday boost window.

        We refresh to the stronger of the current or incoming boost instead
        of stacking multiple deposits indefinitely.
        """
        effective_boost = max(0.0, float(boost))
        if effective_boost <= 0.0:
            return

        self.boost = max(self.boost, effective_boost)
        self.daily_decay = self.boost / float(cfg.active_days)
        self.days_left = int(cfg.active_days)

    def advance(self) -> float:
        """Return today's multiplier and decay the state by one day."""
        if self.days_left <= 0 or self.boost <= 0.0:
            self.boost = 0.0
            self.daily_decay = 0.0
            self.days_left = 0
            return 1.0

        multiplier = 1.0 + self.boost

        self.days_left -= 1
        self.boost = max(0.0, self.boost - self.daily_decay)

        if self.days_left <= 0:
            self.boost = 0.0
            self.daily_decay = 0.0
            self.days_left = 0

        return multiplier
