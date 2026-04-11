"""
Student loan product.

Student loans have unique behavioral signatures: deferment periods,
income-driven repayment plans, and seasonal patterns around
graduation dates.

Statistics:
- ~45M borrowers held more than $1.6T in federal student loans at the
  end of FY 2024 (FSA FY 2024 Annual Report)
- Standard Repayment Plan: fixed payments for up to 10 years for most
  loans, and up to 30 years for consolidation loans (StudentAid.gov)
- IDR forgiveness generally occurs after 20 or 25 years of qualifying
  repayment, depending on plan / borrower status (StudentAid.gov)
- Most federal student loans have a 6-month grace period after leaving
  school before payments begin (StudentAid.gov)
- Historical cohort default rates around ~10% are useful context, but
  they are not direct monthly miss-payment probabilities

Ownership conditioned on persona:
- student: 60% (many in deferment — only a minority actively paying)
- retired: 2% (rare but present among older borrowers)
- salaried: 25%
- freelancer: 20%
- smallbiz: 15%
- hnw: 5%

Modeling notes:
- This module defines the contractual monthly due event.
- Actual realized payment behavior (on-time, late, missed, partial, cure)
  is applied later in transfers/obligations.py.
- This model is primarily federal-student-loan flavored. It uses
  standard / extended / idr_like plan types and a 6-month grace period.
- Broad deferment / forbearance percentages have been unusually distorted
  in recent years by policy and litigation, so this file models
  in-school / pre-repayment deferment directly rather than trying to
  reproduce every temporary relief program.
"""

from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime

from common.channels import STUDENT_LOAN_PAYMENT
from common.date_math import add_months, clip_half_open, month_starts
from common.persona_names import FREELANCER, HNW, RETIRED, SALARIED, SMALLBIZ, STUDENT
from common.validate import between, ge, gt

from .event import Direction, ObligationEvent

_PLAN_TYPES: frozenset[str] = frozenset({"standard", "extended", "idr_like"})


@dataclass(frozen=True, slots=True)
class StudentLoanTerms:
    """
    Contractual parameters and payment-behavior knobs for one student loan.

    Notes:
    - repayment_start_date is the first date repayment is contractually due
    - deferment_end_date can describe a pre-repayment deferment period or a
      later temporary relief period
    - maturity_date / term_months make the loan end, which fixes the old
      "student loans never mature" problem
    """

    monthly_payment: float
    payment_day: int
    servicer_acct: str

    origination_date: datetime
    repayment_start_date: datetime
    term_months: int
    plan_type: str = "standard"
    maturity_date: datetime | None = None
    deferment_end_date: datetime | None = None

    # Behavioral knobs consumed later by transfers/obligations.py
    late_p: float = 0.09
    late_days_min: int = 1
    late_days_max: int = 15
    miss_p: float = 0.03
    partial_p: float = 0.05
    cure_p: float = 0.45
    cluster_mult: float = 2.00
    partial_min_frac: float = 0.30
    partial_max_frac: float = 0.75
    max_cure_cycles: int = 3

    def __post_init__(self) -> None:
        gt("monthly_payment", self.monthly_payment, 0.0)
        between("payment_day", self.payment_day, 1, 28)

        ge("term_months", self.term_months, 1)
        if self.plan_type not in _PLAN_TYPES:
            raise ValueError(
                f"plan_type must be one of {_PLAN_TYPES}, got {self.plan_type!r}"
            )

        if self.repayment_start_date < self.origination_date:
            raise ValueError("repayment_start_date must be >= origination_date")

        if (
            self.maturity_date is not None
            and self.maturity_date <= self.repayment_start_date
        ):
            raise ValueError("maturity_date must be > repayment_start_date")

        if (
            self.deferment_end_date is not None
            and self.deferment_end_date < self.origination_date
        ):
            raise ValueError("deferment_end_date must be >= origination_date")

        between("late_p", self.late_p, 0.0, 1.0)
        ge("late_days_min", self.late_days_min, 0)
        ge("late_days_max", self.late_days_max, self.late_days_min)

        between("miss_p", self.miss_p, 0.0, 1.0)
        between("partial_p", self.partial_p, 0.0, 1.0)
        between("cure_p", self.cure_p, 0.0, 1.0)

        gt("cluster_mult", self.cluster_mult, 0.0)
        between("partial_min_frac", self.partial_min_frac, 0.0, 1.0)
        between("partial_max_frac", self.partial_max_frac, self.partial_min_frac, 1.0)
        ge("max_cure_cycles", self.max_cure_cycles, 1)

    @property
    def in_deferment(self) -> bool:
        """
        Backward-compatible flag for older code paths.

        Interpreted here as "this loan has a configured deferment / pre-repayment
        phase" rather than a dynamic 'is deferred right now' computation.
        """
        return self.deferment_end_date is not None

    @property
    def contractual_end_excl(self) -> datetime:
        if self.maturity_date is not None:
            return self.maturity_date
        return add_months(self.repayment_start_date, self.term_months)


@dataclass(frozen=True, slots=True)
class StudentLoanConfig:
    """
    Population-level parameters for student-loan assignment.

    Research anchors:
    - payment_median keeps the existing synthetic payment anchor
    - grace_period_months follows the common federal 6-month grace period
    - standard / extended / idr_like terms reflect the official repayment
      structures more closely than the repo's old open-ended design
    - student_deferment_p is specifically an in-school / pre-repayment
      deferment prior for the student persona
    """

    payment_median: float = 337.0
    payment_sigma: float = 0.55

    # Fraction of student-persona holders still in school / pre-repayment deferment.
    student_deferment_p: float = 0.60

    # Federal-style grace period after leaving school.
    grace_period_months: int = 6

    # Plan mix: these are modeling choices, not exact official portfolio shares.
    standard_plan_p: float = 0.60
    extended_plan_p: float = 0.10
    idr_like_plan_p: float = 0.30

    # Standard federal repayment structure.
    standard_term_months: int = 120

    # Extended repayment is typically up to 25 years.
    extended_term_months: int = 300

    # IDR-like loans can resolve on 20- or 25-year horizons.
    idr_20_year_p: float = 0.55
    idr_20_year_term_months: int = 240
    idr_25_year_term_months: int = 300

    late_p: float = 0.09
    late_days_min: int = 1
    late_days_max: int = 15

    miss_p: float = 0.03
    partial_p: float = 0.05
    cure_p: float = 0.45
    cluster_mult: float = 2.00
    partial_min_frac: float = 0.30
    partial_max_frac: float = 0.75
    max_cure_cycles: int = 3

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
        ge("grace_period_months", self.grace_period_months, 0)

        between("standard_plan_p", self.standard_plan_p, 0.0, 1.0)
        between("extended_plan_p", self.extended_plan_p, 0.0, 1.0)
        between("idr_like_plan_p", self.idr_like_plan_p, 0.0, 1.0)
        if self.standard_plan_p + self.extended_plan_p + self.idr_like_plan_p <= 0.0:
            raise ValueError("student-loan plan probabilities must sum to > 0")

        ge("standard_term_months", self.standard_term_months, 1)
        ge("extended_term_months", self.extended_term_months, 1)
        ge("idr_20_year_term_months", self.idr_20_year_term_months, 1)
        ge("idr_25_year_term_months", self.idr_25_year_term_months, 1)
        between("idr_20_year_p", self.idr_20_year_p, 0.0, 1.0)

        between("late_p", self.late_p, 0.0, 1.0)
        ge("late_days_min", self.late_days_min, 0)
        ge("late_days_max", self.late_days_max, self.late_days_min)

        between("miss_p", self.miss_p, 0.0, 1.0)
        between("partial_p", self.partial_p, 0.0, 1.0)
        between("cure_p", self.cure_p, 0.0, 1.0)

        gt("cluster_mult", self.cluster_mult, 0.0)
        between("partial_min_frac", self.partial_min_frac, 0.0, 1.0)
        between("partial_max_frac", self.partial_max_frac, self.partial_min_frac, 1.0)
        ge("max_cure_cycles", self.max_cure_cycles, 1)


DEFAULT_STUDENT_LOAN_CONFIG = StudentLoanConfig()

CHANNEL = STUDENT_LOAN_PAYMENT


def scheduled_events(
    person_id: str,
    terms: StudentLoanTerms,
    start: datetime,
    end_excl: datetime,
) -> Iterator[ObligationEvent]:
    """
    Yield contractual monthly student-loan due events within the active repayment window.

    Important:
    - This emits the due schedule only.
    - Realized payment timing variability and delinquency behavior are applied
      later by transfers/obligations.py.
    """
    payment_day = min(terms.payment_day, 28)

    active_start = terms.repayment_start_date
    if terms.deferment_end_date is not None:
        active_start = max(active_start, terms.deferment_end_date)

    active_end_excl = terms.contractual_end_excl

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
