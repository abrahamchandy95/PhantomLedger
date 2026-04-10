"""
Government payment parameters.

Sources:
- SSA Monthly Snapshot Feb 2026: avg retired worker benefit $2,076/month
- SSA: payments arrive on 2nd/3rd/4th Wednesday based on birth date
- ~86% of retired men, ~74% of retired women receive SS
- Disability benefits: avg $1,583/month (Bankrate 2025)
"""

from dataclasses import dataclass

from common.validate import between, ge, gt


@dataclass(frozen=True, slots=True)
class Government:
    # Social Security for retirees
    ss_enabled: bool = True
    ss_eligible_p: float = 0.82  # blended M/F receipt rate
    ss_median: float = 2076.0
    ss_sigma: float = 0.30
    ss_floor: float = 900.0

    # Disability benefits (applied to a small fraction of non-retired)
    disability_enabled: bool = True
    disability_p: float = 0.04  # ~4% of working-age adults
    disability_median: float = 1583.0
    disability_sigma: float = 0.25
    disability_floor: float = 500.0

    def __post_init__(self) -> None:
        between("ss_eligible_p", self.ss_eligible_p, 0.0, 1.0)
        gt("ss_median", self.ss_median, 0.0)
        ge("ss_sigma", self.ss_sigma, 0.0)
        ge("ss_floor", self.ss_floor, 0.0)

        between("disability_p", self.disability_p, 0.0, 1.0)
        gt("disability_median", self.disability_median, 0.0)
        ge("disability_sigma", self.disability_sigma, 0.0)
        ge("disability_floor", self.disability_floor, 0.0)
