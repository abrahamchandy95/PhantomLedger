"""
Mortgage loan product.

A mortgage generates fixed monthly payments from the borrower to
the lender's servicing account. The payment is constant (fixed-rate)
or adjustable, and includes principal + interest + escrow.

Statistics:
- 65% of US homeowners have a mortgage (Census AHS 2023)
- Median monthly payment: $1,672 (Census AHS 2023)
- Average term: 30 years, but effective duration ~7-10 years
  due to refinancing and moves (Freddie Mac 2024)
- Payment day: typically 1st of month, with 15-day grace period
- Late payment rate: ~3.5% of loans 30+ days delinquent (MBA Q3 2024)

Ownership is conditioned on persona:
- student: 0% (renters or dependents)
- retired: 35% (many paid off)
- salaried: 55%
- freelancer: 35%
- smallbiz: 50%
- hnw: 70% (larger homes, investment properties)

Modeling notes:
- This module defines the contractual monthly due event.
- Actual realized payment behavior (on-time, late, missed, partial, cure)
  is applied later in transfers/obligations.py.
- The late-payment parameters below are calibration anchors informed by
  observed delinquency prevalence; they are not meant to imply that
  3.5% of mortgages go late every single month independently.
- The monthly_payment modeled here is the borrower's total contractual
  monthly obligation. If your payment anchor already includes escrow,
  the builder should not add escrow again on top.
"""

from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime

from common.channels import MORTGAGE_PAYMENT
from common.date_math import add_months, clip_half_open, month_starts
from common.persona_names import FREELANCER, HNW, RETIRED, SALARIED, SMALLBIZ, STUDENT
from common.validate import between, ge, gt

from .event import Direction, ObligationEvent


@dataclass(frozen=True, slots=True)
class MortgageTerms:
    """
    Contractual parameters and payment-behavior knobs for one mortgage.

    Notes:
    - payment_day is validated in [1, 28] for calendar safety
    - builder logic should bias real mortgages heavily toward day 1
    - late-day behavior represents the observed payment posting after
      the contractual due date, not a different contractual due date
    """

    monthly_payment: float
    payment_day: int
    lender_acct: str
    start_date: datetime
    term_months: int

    # Behavioral parameters consumed by transfers/obligations.py
    late_p: float
    late_days_min: int
    late_days_max: int
    miss_p: float
    partial_p: float
    cure_p: float
    cluster_mult: float
    partial_min_frac: float
    partial_max_frac: float
    max_cure_cycles: int = 2

    def __post_init__(self) -> None:
        gt("monthly_payment", self.monthly_payment, 0.0)
        between("payment_day", self.payment_day, 1, 28)
        ge("term_months", self.term_months, 1)

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
class MortgageConfig:
    """
    Population-level mortgage assignment parameters.

    Research anchors:
    - payment_median: Census AHS 2023 median monthly mortgage payment
    - ownership rates: persona-level simplification of who is likely
      to hold a mortgage in this synthetic world
    - late_p: anchored to observed serious/30+ day delinquency prevalence
    - escrow_fraction: descriptive share for future decomposition of
      total payment into P&I vs escrow, not necessarily an additive bump
      if payment_median already represents the all-in payment
    """

    # Census AHS 2023 median monthly mortgage payment.
    payment_median: float = 1672.0
    payment_sigma: float = 0.45

    # Delinquency / payment-behavior knobs used by transfers/obligations.py.
    #
    # Mortgage delinquency is low relative to other consumer installment debt.
    # We use the ~3.5% 30+ day delinquency observation as an anchor for
    # "messiness exists but is uncommon" behavior, then let the obligation
    # emitter translate this into late / missed / partial / cure events.
    late_p: float = 0.035

    # Mortgages are typically due on the 1st with a grace period that often
    # runs about 15 days before a late fee. These fields control realized
    # posting delay when a payment is late.
    late_days_min: int = 1
    late_days_max: int = 15

    # Missed and partial payments are intentionally rarer than simple lateness.
    miss_p: float = 0.003
    partial_p: float = 0.002

    # Once behind, many borrowers eventually cure rather than remain in a
    # permanently rolling delinquent state, especially for mortgages.
    cure_p: float = 0.70
    cluster_mult: float = 1.60
    partial_min_frac: float = 0.40
    partial_max_frac: float = 0.85
    max_cure_cycles: int = 2

    # Typical share of the all-in payment attributable to escrow-like items.
    # Keep for documentation / future decomposition. The builder should avoid
    # multiplying payment_median by this again if payment_median is already
    # intended to be the total monthly obligation.
    escrow_fraction: float = 0.30

    rates: dict[str, float] | None = None

    def ownership_p(self, persona: str) -> float:
        """
        Persona-level mortgage ownership prior.

        Preserves the research assumptions documented in the module header.
        """
        defaults: dict[str, float] = {
            STUDENT: 0.00,
            RETIRED: 0.35,
            SALARIED: 0.55,
            FREELANCER: 0.35,
            SMALLBIZ: 0.50,
            HNW: 0.70,
        }
        if self.rates is not None:
            return self.rates.get(persona, defaults.get(persona, 0.0))
        return defaults.get(persona, 0.0)

    def __post_init__(self) -> None:
        gt("payment_median", self.payment_median, 0.0)
        ge("payment_sigma", self.payment_sigma, 0.0)

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

        between("escrow_fraction", self.escrow_fraction, 0.0, 1.0)


DEFAULT_MORTGAGE_CONFIG = MortgageConfig()

CHANNEL = MORTGAGE_PAYMENT


def scheduled_events(
    person_id: str,
    terms: MortgageTerms,
    start: datetime,
    end_excl: datetime,
) -> Iterator[ObligationEvent]:
    """
    Yield contractual monthly mortgage due events within the active loan window.

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
            hour=6,
            minute=0,
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
            product_type="mortgage",
        )
