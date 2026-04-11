"""
Auto loan product.

Auto loans generate fixed monthly payments, typically via ACH
autopay to the lender's servicing account.

Statistics:
- 85% of new cars and 55% of used cars are financed (Experian Q3 2024)
- Average new car payment: $734/month (Experian Q3 2024)
- Average used car payment: $525/month (Experian Q3 2024)
- Average loan term: 68 months new, 72 months used (Experian Q3 2024)
- Delinquency rate (60+ days): 2.1% (Fed NY Q4 2024)
- ~107M Americans have auto loans (Experian 2024)

Ownership conditioned on persona:
- student: 15% (some have used-car loans)
- retired: 25%
- salaried: 45%
- freelancer: 40%
- smallbiz: 50%
- hnw: 30% (often pay cash or lease)

Modeling notes:
- This module defines the contractual monthly due event.
- Actual realized payment behavior (on-time, late, missed, partial, cure)
  is applied later in transfers/obligations.py.
- The new/used financing statistics above are purchase/origination-style
  anchors. In this synthetic world, persona ownership rates remain separate
  stock-style priors, while the builder uses a new-vs-used segment mix to
  sample payment and term characteristics.
- The 60+ day delinquency statistic is stricter than general "late payment"
  behavior, so the delinquency knobs below represent a broader repayment
  surface rather than a direct one-to-one copy of that single metric.
"""

from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime

from common.channels import AUTO_LOAN_PAYMENT
from common.date_math import add_months, clip_half_open, month_starts
from common.persona_names import FREELANCER, HNW, RETIRED, SALARIED, SMALLBIZ, STUDENT
from common.validate import between, ge, gt

from .event import Direction, ObligationEvent

_AUTO_SEGMENTS: frozenset[str] = frozenset({"new", "used"})


@dataclass(frozen=True, slots=True)
class AutoLoanTerms:
    """
    Contractual parameters and payment-behavior knobs for one auto loan.

    Notes:
    - payment_day is validated in [1, 28] for calendar safety
    - vehicle_segment distinguishes new vs used loans so the builder can
      preserve the research-backed payment and term differences
    - delinquency behavior is realized later by transfers/obligations.py
    """

    monthly_payment: float
    payment_day: int
    lender_acct: str
    start_date: datetime
    term_months: int
    vehicle_segment: str = "used"

    # Behavioral parameters consumed by transfers/obligations.py
    late_p: float = 0.04
    late_days_min: int = 1
    late_days_max: int = 10
    miss_p: float = 0.01
    partial_p: float = 0.02
    cure_p: float = 0.58
    cluster_mult: float = 1.80
    partial_min_frac: float = 0.35
    partial_max_frac: float = 0.80
    max_cure_cycles: int = 2

    def __post_init__(self) -> None:
        gt("monthly_payment", self.monthly_payment, 0.0)
        between("payment_day", self.payment_day, 1, 28)
        ge("term_months", self.term_months, 1)

        if self.vehicle_segment not in _AUTO_SEGMENTS:
            raise ValueError(
                f"vehicle_segment must be one of {_AUTO_SEGMENTS}, "
                + f"got {self.vehicle_segment!r}"
            )

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


@dataclass(frozen=True, slots=True)
class AutoLoanConfig:
    """
    Population-level parameters for auto loan assignment.

    Research anchors:
    - persona ownership rates are stock-style synthetic priors
    - payment and term anchors preserve new-vs-used market differences
    - delinquency knobs are broader repayment-behavior controls, not a
      direct re-expression of a single severe-delinquency stock statistic
    """

    # Ownership prior in the synthetic current-population portfolio.
    rates: dict[str, float] | None = None

    # New-vs-used mix among borrowers who do have an auto loan.
    # This is a modeling choice, not a direct market-share claim.
    # The repo should lean used because used vehicles dominate household stock.
    new_vehicle_share: float = 0.35

    # Payment anchors preserved from the research block.
    new_payment_median: float = 734.0
    new_payment_sigma: float = 0.28
    used_payment_median: float = 525.0
    used_payment_sigma: float = 0.32

    # Term anchors preserved from the research block.
    new_term_mean_months: float = 68.0
    new_term_sigma_months: float = 8.0
    used_term_mean_months: float = 72.0
    used_term_sigma_months: float = 10.0
    term_min_months: int = 36
    term_max_months: int = 84

    # Auto loans are behaviorally messier than mortgages, but still far from
    # revolving-credit chaos. These defaults are intended to produce some
    # observable lateness / misses / partials without overwhelming the ledger.
    late_p: float = 0.04
    late_days_min: int = 1
    late_days_max: int = 10

    miss_p: float = 0.01
    partial_p: float = 0.02
    cure_p: float = 0.58
    cluster_mult: float = 1.80
    partial_min_frac: float = 0.35
    partial_max_frac: float = 0.80
    max_cure_cycles: int = 2

    def ownership_p(self, persona: str) -> float:
        """
        Persona-level auto-loan ownership prior.

        Preserves the research assumptions documented in the module header.
        These are not direct translations of new-vs-used financing rates.
        """
        defaults: dict[str, float] = {
            STUDENT: 0.15,
            RETIRED: 0.25,
            SALARIED: 0.45,
            FREELANCER: 0.40,
            SMALLBIZ: 0.50,
            HNW: 0.30,
        }
        if self.rates is not None:
            return self.rates.get(persona, defaults.get(persona, 0.0))
        return defaults.get(persona, 0.0)

    def payment_params(self, vehicle_segment: str) -> tuple[float, float]:
        if vehicle_segment == "new":
            return self.new_payment_median, self.new_payment_sigma
        if vehicle_segment == "used":
            return self.used_payment_median, self.used_payment_sigma
        raise ValueError(f"unknown vehicle_segment {vehicle_segment!r}")

    def term_params(self, vehicle_segment: str) -> tuple[float, float]:
        if vehicle_segment == "new":
            return self.new_term_mean_months, self.new_term_sigma_months
        if vehicle_segment == "used":
            return self.used_term_mean_months, self.used_term_sigma_months
        raise ValueError(f"unknown vehicle_segment {vehicle_segment!r}")

    def __post_init__(self) -> None:
        between("new_vehicle_share", self.new_vehicle_share, 0.0, 1.0)

        gt("new_payment_median", self.new_payment_median, 0.0)
        ge("new_payment_sigma", self.new_payment_sigma, 0.0)
        gt("used_payment_median", self.used_payment_median, 0.0)
        ge("used_payment_sigma", self.used_payment_sigma, 0.0)

        gt("new_term_mean_months", self.new_term_mean_months, 0.0)
        ge("new_term_sigma_months", self.new_term_sigma_months, 0.0)
        gt("used_term_mean_months", self.used_term_mean_months, 0.0)
        ge("used_term_sigma_months", self.used_term_sigma_months, 0.0)
        ge("term_min_months", self.term_min_months, 1)
        ge("term_max_months", self.term_max_months, self.term_min_months)

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


DEFAULT_AUTO_LOAN_CONFIG = AutoLoanConfig()

CHANNEL = AUTO_LOAN_PAYMENT


def scheduled_events(
    person_id: str,
    terms: AutoLoanTerms,
    start: datetime,
    end_excl: datetime,
) -> Iterator[ObligationEvent]:
    """
    Yield contractual monthly auto-loan due events within the active contract window.

    Important:
    - This emits the due schedule only.
    - Realized payment timing variability and delinquency behavior are applied
      later by transfers/obligations.py.
    """
    payment_day = min(terms.payment_day, 28)

    active_start = terms.start_date
    active_end_excl = add_months(terms.start_date, terms.term_months)

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
            hour=7,
            minute=30,
            second=0,
            microsecond=0,
        )

        if ts < effective_start or ts >= effective_end_excl:
            continue

        yield ObligationEvent(
            person_id=person_id,
            direction=Direction.OUTFLOW,
            counterparty_acct=terms.lender_acct,
            amount=terms.monthly_payment,
            timestamp=ts,
            channel=CHANNEL,
            product_type="auto_loan",
        )
