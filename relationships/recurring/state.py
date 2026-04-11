from collections.abc import Callable
from dataclasses import dataclass
from datetime import datetime
from enum import StrEnum

from common.random import RngFactory
from common.validate import ge, gt

type SalarySource = Callable[[], float]
type RentSource = Callable[[], float]
type SeedSource = RngFactory | int


class PayCadence(StrEnum):
    WEEKLY = "weekly"
    BIWEEKLY = "biweekly"
    SEMIMONTHLY = "semimonthly"
    MONTHLY = "monthly"


@dataclass(frozen=True, slots=True)
class PayrollProfile:
    cadence: PayCadence
    anchor_date: datetime
    weekday: int = 4
    semimonthly_days: tuple[int, int] = (15, 31)
    monthly_day: int = 31
    weekend_roll: str = "previous_business_day"
    posting_lag_days: int = 0

    def __post_init__(self) -> None:
        if not 0 <= int(self.weekday) <= 6:
            raise ValueError(f"weekday must be in [0, 6], got {self.weekday}")

        first, second = self.semimonthly_days
        if not 1 <= int(first) <= 31:
            raise ValueError(f"semimonthly_days[0] must be in [1, 31], got {first}")
        if not int(first) <= int(second) <= 31:
            raise ValueError(
                "semimonthly_days[1] must be >= semimonthly_days[0] and <= 31"
            )

        if not 1 <= int(self.monthly_day) <= 31:
            raise ValueError(f"monthly_day must be in [1, 31], got {self.monthly_day}")

        if self.weekend_roll not in {
            "previous_business_day",
            "next_business_day",
            "none",
        }:
            raise ValueError(
                "weekend_roll must be one of "
                + "{'previous_business_day', 'next_business_day', 'none'}"
            )

        ge("posting_lag_days", self.posting_lag_days, 0)


@dataclass(slots=True)
class Employment:
    """Tracks the lifecycle and current state of a person's job."""

    employer_acct: str
    payroll: PayrollProfile
    start: datetime
    end: datetime
    annual_salary: float
    switch_index: int

    def __post_init__(self) -> None:
        gt("annual_salary", self.annual_salary, 0.0)


@dataclass(slots=True)
class Lease:
    """Tracks the lifecycle and current state of a person's housing lease."""

    landlord_acct: str
    start: datetime
    end: datetime
    base_rent: float
    move_index: int
