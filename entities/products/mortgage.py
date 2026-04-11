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
"""

from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime

from common.date_math import add_months, clip_half_open, month_starts
from common.persona_names import FREELANCER, HNW, RETIRED, SALARIED, SMALLBIZ, STUDENT
from common.validate import between, ge, gt

from .event import Direction, ObligationEvent


@dataclass(frozen=True, slots=True)
class MortgageTerms:
    """The contractual parameters of a single mortgage."""

    monthly_payment: float
    payment_day: int
    lender_acct: str
    start_date: datetime
    term_months: int

    def __post_init__(self) -> None:
        gt("monthly_payment", self.monthly_payment, 0.0)
        between("payment_day", self.payment_day, 1, 28)
        ge("term_months", self.term_months, 1)


@dataclass(frozen=True, slots=True)
class MortgageConfig:
    """Population-level parameters for mortgage assignment."""

    # Median monthly payment (Census AHS 2023)
    payment_median: float = 1672.0
    payment_sigma: float = 0.45

    # Late payment probability per month (MBA Q3 2024: ~3.5%)
    late_p: float = 0.035
    late_days_min: int = 1
    late_days_max: int = 14

    # Escrow bump: property tax + insurance bundled into payment
    # Adds ~25-35% on top of P+I (Bankrate 2025)
    escrow_fraction: float = 0.30

    # Persona ownership probabilities
    rates: dict[str, float] | None = None

    def ownership_p(self, persona: str) -> float:
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
        between("escrow_fraction", self.escrow_fraction, 0.0, 1.0)


DEFAULT_MORTGAGE_CONFIG = MortgageConfig()

CHANNEL = "mortgage_payment"


def scheduled_events(
    person_id: str,
    terms: MortgageTerms,
    start: datetime,
    end_excl: datetime,
) -> Iterator[ObligationEvent]:
    """Yield monthly mortgage payments within the active contract window."""
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
