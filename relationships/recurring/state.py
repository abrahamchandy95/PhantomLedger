from collections.abc import Callable
from dataclasses import dataclass
from datetime import datetime

from common.random import RngFactory

type SalarySource = Callable[[], float]
type RentSource = Callable[[], float]
type SeedSource = RngFactory | int


@dataclass(slots=True)
class Employment:
    """Tracks the lifecycle and current state of a person's job."""

    employer_acct: str
    start: datetime
    end: datetime
    base_salary: float
    switch_index: int


@dataclass(slots=True)
class Lease:
    """Tracks the lifecycle and current state of a person's housing lease."""

    landlord_acct: str
    start: datetime
    end: datetime
    base_rent: float
    move_index: int
