"""
Tax withholding and estimated payment profile.

For W-2 earners, federal/state taxes are withheld from each
paycheck — this is invisible in bank transaction data because
the net deposit already reflects withholding. But for freelancers
and small business owners, quarterly estimated tax payments are
*visible* outflows to the IRS/state treasury.

Statistics:
- ~16.5M taxpayers file Schedule C (IRS SOI 2022)
- Quarterly estimated payments due: Apr 15, Jun 15, Sep 15, Jan 15
- Average quarterly payment: ~$3,500 for self-employed (IRS 2023)
- Penalty for underpayment: ~8% annual rate (IRS 2025)

Additionally, ~75% of filers receive a refund averaging $3,138
(IRS 2024 filing season). Refunds create a visible seasonal
inflow, typically Feb-May.

Ownership:
- student: 0% (no estimated payments)
- retired: 10% (pension withholding gaps)
- salaried: 0% (withholding handled by employer)
- freelancer: 85%
- smallbiz: 90%
- hnw: 70% (investment income, K-1s)
"""

from dataclasses import dataclass
from datetime import datetime
from collections.abc import Iterator

from common.persona_names import STUDENT, RETIRED, SALARIED, FREELANCER, SMALLBIZ, HNW
from common.validate import between, ge, gt

from .event import Direction, ObligationEvent


# IRS quarterly due dates (month, day)
_QUARTERLY_DATES: tuple[tuple[int, int], ...] = (
    (1, 15),  # Q4 prior year
    (4, 15),  # Q1
    (6, 15),  # Q2
    (9, 15),  # Q3
)


@dataclass(frozen=True, slots=True)
class TaxTerms:
    """Per-person tax payment parameters."""

    quarterly_amount: float
    treasury_acct: str

    # Refund (0.0 if none expected)
    refund_amount: float = 0.0
    refund_month: int = 3  # typical: Feb-Apr

    def __post_init__(self) -> None:
        ge("quarterly_amount", self.quarterly_amount, 0.0)
        ge("refund_amount", self.refund_amount, 0.0)
        between("refund_month", self.refund_month, 1, 12)


@dataclass(frozen=True, slots=True)
class TaxConfig:
    """Population-level parameters for tax profile assignment."""

    # Quarterly estimated payment (IRS 2023 self-employed avg)
    quarterly_median: float = 3500.0
    quarterly_sigma: float = 0.60

    # Refund parameters (IRS 2024 filing season)
    refund_p: float = 0.75
    refund_median: float = 3138.0
    refund_sigma: float = 0.50

    rates: dict[str, float] | None = None

    def ownership_p(self, persona: str) -> float:
        defaults: dict[str, float] = {
            STUDENT: 0.00,
            RETIRED: 0.10,
            SALARIED: 0.00,
            FREELANCER: 0.85,
            SMALLBIZ: 0.90,
            HNW: 0.70,
        }
        if self.rates is not None:
            return self.rates.get(persona, defaults.get(persona, 0.0))
        return defaults.get(persona, 0.0)

    def __post_init__(self) -> None:
        gt("quarterly_median", self.quarterly_median, 0.0)
        ge("quarterly_sigma", self.quarterly_sigma, 0.0)
        between("refund_p", self.refund_p, 0.0, 1.0)


DEFAULT_TAX_CONFIG = TaxConfig()

CHANNEL_PAYMENT = "tax_estimated_payment"
CHANNEL_REFUND = "tax_refund"


def scheduled_events(
    person_id: str,
    terms: TaxTerms,
    start: datetime,
    end_excl: datetime,
) -> Iterator[ObligationEvent]:
    """Yield quarterly tax payments and annual refund within the window."""
    # Quarterly estimated payments
    if terms.quarterly_amount > 0.0:
        for year in range(start.year, end_excl.year + 1):
            for month, day in _QUARTERLY_DATES:
                ts = datetime(year, month, day, 10, 0, 0)
                if ts >= start and ts < end_excl:
                    yield ObligationEvent(
                        person_id=person_id,
                        direction=Direction.OUTFLOW,
                        counterparty_acct=terms.treasury_acct,
                        amount=terms.quarterly_amount,
                        timestamp=ts,
                        channel=CHANNEL_PAYMENT,
                        product_type="tax",
                    )

    # Annual refund
    if terms.refund_amount > 0.0:
        for year in range(start.year, end_excl.year + 1):
            ts = datetime(year, terms.refund_month, 15, 12, 0, 0)
            if ts >= start and ts < end_excl:
                yield ObligationEvent(
                    person_id=person_id,
                    direction=Direction.INFLOW,
                    counterparty_acct=terms.treasury_acct,
                    amount=terms.refund_amount,
                    timestamp=ts,
                    channel=CHANNEL_REFUND,
                    product_type="tax",
                )
