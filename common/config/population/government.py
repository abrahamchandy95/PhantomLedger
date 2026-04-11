"""
Government benefit parameters.

Research notes for these defaults:
- SSA 2026 COLA fact sheet: estimated January 2026 average monthly
  retirement benefit for retired workers is $2,071, and for disabled
  workers is $1,630.
- SSA Dec. 31, 2025 fact sheet: about 87% of people age 65+ receive
  Social Security benefits.
- SSA payment calendars and handbook: post-1997 RSDI payments are
  scheduled by birth-date cohort on the 2nd/3rd/4th Wednesday of
  each month (1st-10th, 11th-20th, 21st-31st).
- This repo does not model the main "paid on the 3rd" exceptions
  (for example: pre-May-1997 entitlement, dual SSI entitlement,
  certain foreign-resident cases). The transfer generator therefore
  uses the standard Wednesday-cycle rule for all SSA-style benefits.
"""

from dataclasses import dataclass

from common.validate import between, ge, gt


@dataclass(frozen=True, slots=True)
class Government:
    # Social Security for retirees.
    # SSA Dec. 31, 2025 fact sheet: ~87% of people age 65+ receive benefits.
    # We use that as a simple retiree eligibility prior in this synthetic world.
    ss_enabled: bool = True
    ss_eligible_p: float = 0.87
    # SSA 2026 COLA fact sheet: estimated Jan. 2026 average retired-worker
    # benefit is $2,071/month. We use that as the lognormal anchor.
    ss_median: float = 2071.0
    ss_sigma: float = 0.30
    ss_floor: float = 900.0

    # Disability benefits (applied to a small fraction of non-retired).
    disability_enabled: bool = True
    disability_p: float = 0.04
    # SSA 2026 COLA fact sheet: estimated Jan. 2026 average disabled-worker
    # benefit is $1,630/month. We use that as the lognormal anchor.
    disability_median: float = 1630.0
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
