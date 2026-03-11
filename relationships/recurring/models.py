from dataclasses import dataclass
from datetime import datetime


@dataclass(slots=True)
class EmploymentState:
    employer_acct: str
    start: datetime
    end: datetime
    base_salary: float
    switch_index: int


@dataclass(slots=True)
class LeaseState:
    landlord_acct: str
    start: datetime
    end: datetime
    base_rent: float
    move_index: int

