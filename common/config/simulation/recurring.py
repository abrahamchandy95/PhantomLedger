from dataclasses import dataclass

from common.validate import (
    require_float_between,
    require_float_ge,
    require_float_gt,
)


@dataclass(frozen=True, slots=True)
class RecurringConfig:
    salary_fraction: float = 0.65
    rent_fraction: float = 0.55

    employer_tenure_years_min: float = 2.0
    employer_tenure_years_max: float = 10.0
    landlord_tenure_years_min: float = 2.0
    landlord_tenure_years_max: float = 10.0

    annual_inflation_rate: float = 0.03

    salary_real_raise_mu: float = 0.02
    salary_real_raise_sigma: float = 0.01
    salary_real_raise_floor: float = 0.005

    job_switch_bump_mu: float = 0.08
    job_switch_bump_sigma: float = 0.05
    job_switch_bump_floor: float = 0.00

    rent_real_raise_mu: float = 0.03
    rent_real_raise_sigma: float = 0.02
    rent_real_raise_floor: float = 0.00

    def validate(self) -> None:
        require_float_between("salary_fraction", self.salary_fraction, 0.0, 1.0)
        require_float_between("rent_fraction", self.rent_fraction, 0.0, 1.0)

        require_float_gt(
            "employer_tenure_years_min",
            self.employer_tenure_years_min,
            0.0,
        )
        if float(self.employer_tenure_years_max) < float(
            self.employer_tenure_years_min
        ):
            raise ValueError(
                "employer_tenure_years_max must be >= employer_tenure_years_min"
            )

        require_float_gt(
            "landlord_tenure_years_min",
            self.landlord_tenure_years_min,
            0.0,
        )
        if float(self.landlord_tenure_years_max) < float(
            self.landlord_tenure_years_min
        ):
            raise ValueError(
                "landlord_tenure_years_max must be >= landlord_tenure_years_min"
            )

        require_float_ge("annual_inflation_rate", self.annual_inflation_rate, 0.0)

        require_float_ge("salary_real_raise_sigma", self.salary_real_raise_sigma, 0.0)
        require_float_ge("job_switch_bump_sigma", self.job_switch_bump_sigma, 0.0)
        require_float_ge("rent_real_raise_sigma", self.rent_real_raise_sigma, 0.0)
