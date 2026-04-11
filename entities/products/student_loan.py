"""
Student loan product.

Student loans have unique behavioral signatures: deferment periods,
income-driven repayment plans, and seasonal patterns around
graduation dates.

Statistics:
- 43.2M borrowers owe $1.77T total (Fed Reserve Q4 2024)
- Average monthly payment: $337 (BLS 2024)
- Median balance: $28,950 (Education Data Initiative 2025)
- Default rate: 10.8% within 3 years of entering repayment (FSA 2024)
- ~23% of borrowers are in deferment/forbearance at any time

Ownership conditioned on persona:
- student: 60% (many in deferment — only ~40% of those actively paying)
- retired: 2% (rare but growing: 3.5M borrowers 60+)
- salaried: 25%
- freelancer: 20%
- smallbiz: 15%
- hnw: 5%
"""

from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime

from common.date_math import clip_half_open, month_starts
from common.persona_names import FREELANCER, HNW, RETIRED, SALARIED, SMALLBIZ, STUDENT
from common.validate import between, ge, gt

from .event import Direction, ObligationEvent


@dataclass(frozen=True, slots=True)
class StudentLoanTerms:
    """The contractual parameters of a single student loan."""

    monthly_payment: float
    payment_day: int
    servicer_acct: str
    start_date: datetime
    in_deferment: bool = False

    def __post_init__(self) -> None:
        gt("monthly_payment", self.monthly_payment, 0.0)
        between("payment_day", self.payment_day, 1, 28)


@dataclass(frozen=True, slots=True)
class StudentLoanConfig:
    """Population-level parameters for student loan assignment."""

    payment_median: float = 337.0
    payment_sigma: float = 0.55

    # Fraction of student-persona holders in deferment
    # (not actively paying — only accruing interest)
    student_deferment_p: float = 0.60

    late_p: float = 0.08
    late_days_min: int = 1
    late_days_max: int = 15

    rates: dict[str, float] | None = None

    def ownership_p(self, persona: str) -> float:
        defaults: dict[str, float] = {
            STUDENT: 0.60,
            RETIRED: 0.02,
            SALARIED: 0.25,
            FREELANCER: 0.20,
            SMALLBIZ: 0.15,
            HNW: 0.05,
        }
        if self.rates is not None:
            return self.rates.get(persona, defaults.get(persona, 0.0))
        return defaults.get(persona, 0.0)

    def __post_init__(self) -> None:
        gt("payment_median", self.payment_median, 0.0)
        ge("payment_sigma", self.payment_sigma, 0.0)
        between("student_deferment_p", self.student_deferment_p, 0.0, 1.0)
        between("late_p", self.late_p, 0.0, 1.0)


DEFAULT_STUDENT_LOAN_CONFIG = StudentLoanConfig()

CHANNEL = "student_loan_payment"


def scheduled_events(
    person_id: str,
    terms: StudentLoanTerms,
    start: datetime,
    end_excl: datetime,
) -> Iterator[ObligationEvent]:
    """Yield monthly student loan payments within the active repayment window."""
    # Deferred loans emit no payment events
    if terms.in_deferment:
        return

    payment_day = min(terms.payment_day, 28)

    # StudentLoanTerms currently has no maturity field, so this scheduler can
    # enforce origination timing but not a contractual end date yet.
    active_start = terms.start_date
    active_end_excl = None

    clipped = clip_half_open(
        window_start=start,
        window_end_excl=end_excl,
        active_start=active_start,
        active_end_excl=active_end_excl,
    )
    if clipped is None:
        return

    effective_start, effective_end_excl = clipped

    for current in month_starts(effective_start, effective_end_excl):
        ts = current.replace(
            day=payment_day,
            hour=8,
            minute=0,
            second=0,
            microsecond=0,
        )

        if ts < effective_start or ts >= effective_end_excl:
            continue

        yield ObligationEvent(
            person_id=person_id,
            direction=Direction.OUTFLOW,
            counterparty_acct=terms.servicer_acct,
            amount=terms.monthly_payment,
            timestamp=ts,
            channel=CHANNEL,
            product_type="student_loan",
        )
