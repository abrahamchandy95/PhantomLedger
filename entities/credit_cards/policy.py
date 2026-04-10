from dataclasses import dataclass

from common.validate import (
    between,
    ge,
    gt,
)


@dataclass(frozen=True, slots=True)
class IssuancePolicy:
    apr_median: float = 0.22
    apr_sigma: float = 0.25
    apr_min: float = 0.08
    apr_max: float = 0.36

    limit_sigma: float = 0.65

    cycle_day_min: int = 1
    cycle_day_max: int = 28

    autopay_full_p: float = 0.40
    autopay_min_p: float = 0.10

    def __post_init__(self) -> None:
        """Auto-validates configuration on creation."""
        gt("apr_median", self.apr_median, 0.0)
        ge("apr_sigma", self.apr_sigma, 0.0)
        gt("apr_min", self.apr_min, 0.0)
        gt("apr_max", self.apr_max, 0.0)
        if float(self.apr_max) < float(self.apr_min):
            raise ValueError("apr_max must be >= apr_min")

        gt("limit_sigma", self.limit_sigma, 0.0)

        ge("cycle_day_min", self.cycle_day_min, 1)
        ge("cycle_day_max", self.cycle_day_max, self.cycle_day_min)

        between("autopay_full_p", self.autopay_full_p, 0.0, 1.0)
        between("autopay_min_p", self.autopay_min_p, 0.0, 1.0)


# Export a clean default instance
DEFAULT_POLICY = IssuancePolicy()
