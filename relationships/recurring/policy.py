from dataclasses import dataclass

from common.validate import (
    between,
    ge,
    gt,
)


def _require_valid_range(
    *,
    min_name: str,
    min_value: float,
    max_name: str,
    max_value: float,
) -> None:
    """Ensures that the minimum is strictly positive and the maximum is >= minimum."""
    gt(min_name, min_value, 0.0)
    if float(max_value) < float(min_value):
        raise ValueError(f"{max_name} must be >= {min_name}")


@dataclass(frozen=True, slots=True)
class Policy:
    """Defines the statistical parameters and constraints for recurring events."""

    salary_fraction: float = 0.65
    rent_fraction: float = 0.55

    job_tenure_min: float = 2.0
    job_tenure_max: float = 10.0
    lease_tenure_min: float = 2.0
    lease_tenure_max: float = 10.0

    inflation: float = 0.03

    salary_raise_mu: float = 0.02
    salary_raise_sigma: float = 0.01
    salary_raise_floor: float = 0.005

    job_bump_mu: float = 0.08
    job_bump_sigma: float = 0.05
    job_bump_floor: float = 0.00

    rent_raise_mu: float = 0.03
    rent_raise_sigma: float = 0.02
    rent_raise_floor: float = 0.00

    def __post_init__(self) -> None:
        """Guarantees the policy is mathematically valid upon instantiation."""
        between("salary_fraction", self.salary_fraction, 0.0, 1.0)
        between("rent_fraction", self.rent_fraction, 0.0, 1.0)

        _require_valid_range(
            min_name="job_tenure_min",
            min_value=self.job_tenure_min,
            max_name="job_tenure_max",
            max_value=self.job_tenure_max,
        )
        _require_valid_range(
            min_name="lease_tenure_min",
            min_value=self.lease_tenure_min,
            max_name="lease_tenure_max",
            max_value=self.lease_tenure_max,
        )

        ge("inflation", self.inflation, 0.0)
        ge("salary_raise_sigma", self.salary_raise_sigma, 0.0)
        ge("job_bump_sigma", self.job_bump_sigma, 0.0)
        ge("rent_raise_sigma", self.rent_raise_sigma, 0.0)


DEFAULT_POLICY = Policy()
