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
"""

from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime

from common.date_math import add_months, clip_half_open, month_starts
from common.persona_names import FREELANCER, HNW, RETIRED, SALARIED, SMALLBIZ, STUDENT
from common.validate import between, ge, gt

from .event import Direction, ObligationEvent


@dataclass(frozen=True, slots=True)
class AutoLoanTerms:
    """The contractual parameters of a single auto loan."""

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
class AutoLoanConfig:
    """Population-level parameters for auto loan assignment."""

    # Blended median across new ($734) and used ($525)
    # weighted ~40/60 by volume (Experian Q3 2024)
    payment_median: float = 610.0
    payment_sigma: float = 0.40

    late_p: float = 0.021
    late_days_min: int = 1
    late_days_max: int = 10

    rates: dict[str, float] | None = None

    def ownership_p(self, persona: str) -> float:
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

    def __post_init__(self) -> None:
        gt("payment_median", self.payment_median, 0.0)
        ge("payment_sigma", self.payment_sigma, 0.0)
        between("late_p", self.late_p, 0.0, 1.0)


DEFAULT_AUTO_LOAN_CONFIG = AutoLoanConfig()

CHANNEL = "auto_loan_payment"


def scheduled_events(
    person_id: str,
    terms: AutoLoanTerms,
    start: datetime,
    end_excl: datetime,
) -> Iterator[ObligationEvent]:
    """Yield monthly auto loan payments within the active contract window."""
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
