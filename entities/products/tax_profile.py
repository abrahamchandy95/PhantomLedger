"""
Estimated-tax and annual-settlement profile.

For W-2 earners, federal/state withholding is already netted out of the
paycheck deposit, so it is usually invisible in bank transaction data.
Quarterly estimated-tax payments are different: they are visible outflows
and are mainly associated with income streams that lack withholding.

Those are two separate phenomena, so this module models them separately:

1) estimated-tax profile
   - quarterly outflows to Treasury
   - ownership depends strongly on persona/income type

2) annual filing settlement
   - refund, balance due, or no visible settlement event
   - applies broadly across the filing population, including salaried users
"""

from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime

from common.calendar_cache import WindowCalendar
from common.channels import TAX_BALANCE_DUE, TAX_ESTIMATED_PAYMENT, TAX_REFUND
from common.persona_names import FREELANCER, HNW, RETIRED, SALARIED, SMALLBIZ, STUDENT
from common.validate import between, ge, gt

from .event import Direction, ObligationEvent


@dataclass(frozen=True, slots=True)
class TaxTerms:
    """Per-person visible tax cash-flow parameters."""

    treasury_acct: str

    quarterly_amount: float = 0.0

    refund_amount: float = 0.0
    refund_month: int = 2

    balance_due_amount: float = 0.0
    balance_due_month: int = 4

    def __post_init__(self) -> None:
        ge("quarterly_amount", self.quarterly_amount, 0.0)
        ge("refund_amount", self.refund_amount, 0.0)
        ge("balance_due_amount", self.balance_due_amount, 0.0)

        between("refund_month", self.refund_month, 1, 12)
        between("balance_due_month", self.balance_due_month, 1, 12)


@dataclass(frozen=True, slots=True)
class TaxConfig:
    """
    Population-level parameters for visible tax cash flows.

    Modeling notes:
    - Estimated-tax visibility is concentrated in self-employment /
      owner-draw style personas.
    - Annual refunds / balances due are broader and can apply to
      most filers, including salaried households.
    """

    rates: dict[str, float] | None = None

    quarterly_amount_median: float = 1800.0
    quarterly_amount_sigma: float = 0.75

    refund_p: float = 0.55
    refund_amount_median: float = 900.0
    refund_amount_sigma: float = 0.80

    balance_due_p: float = 0.18
    balance_due_amount_median: float = 1200.0
    balance_due_amount_sigma: float = 0.90

    refund_month: int = 2
    balance_due_month: int = 4

    def ownership_p(self, persona: str) -> float:
        defaults: dict[str, float] = {
            STUDENT: 0.02,
            RETIRED: 0.08,
            SALARIED: 0.06,
            FREELANCER: 0.70,
            SMALLBIZ: 0.85,
            HNW: 0.45,
        }
        if self.rates is not None:
            return self.rates.get(persona, defaults.get(persona, 0.0))
        return defaults.get(persona, 0.0)

    def __post_init__(self) -> None:
        gt("quarterly_amount_median", self.quarterly_amount_median, 0.0)
        ge("quarterly_amount_sigma", self.quarterly_amount_sigma, 0.0)

        between("refund_p", self.refund_p, 0.0, 1.0)
        gt("refund_amount_median", self.refund_amount_median, 0.0)
        ge("refund_amount_sigma", self.refund_amount_sigma, 0.0)

        between("balance_due_p", self.balance_due_p, 0.0, 1.0)
        gt("balance_due_amount_median", self.balance_due_amount_median, 0.0)
        ge("balance_due_amount_sigma", self.balance_due_amount_sigma, 0.0)

        between("refund_month", self.refund_month, 1, 12)
        between("balance_due_month", self.balance_due_month, 1, 12)


DEFAULT_TAX_CONFIG = TaxConfig()


def scheduled_events(
    person_id: str,
    terms: TaxTerms,
    start: datetime,
    end_excl: datetime,
    *,
    calendar: WindowCalendar | None = None,
) -> Iterator[ObligationEvent]:
    """Yield quarterly estimated-tax payments and annual settlement events."""
    if calendar is None:
        calendar = WindowCalendar(start, end_excl)

    if terms.quarterly_amount > 0.0:
        for ts in calendar.iter_estimated_tax(start, end_excl):
            yield ObligationEvent(
                person_id=person_id,
                direction=Direction.OUTFLOW,
                counterparty_acct=terms.treasury_acct,
                amount=terms.quarterly_amount,
                timestamp=ts,
                channel=TAX_ESTIMATED_PAYMENT,
                product_type="tax",
            )

    if terms.refund_amount > 0.0:
        for ts in calendar.iter_annual_fixed(
            start,
            end_excl,
            month=terms.refund_month,
            day=15,
            hour=12,
            minute=0,
        ):
            yield ObligationEvent(
                person_id=person_id,
                direction=Direction.INFLOW,
                counterparty_acct=terms.treasury_acct,
                amount=terms.refund_amount,
                timestamp=ts,
                channel=TAX_REFUND,
                product_type="tax",
            )

    if terms.balance_due_amount > 0.0:
        for ts in calendar.iter_annual_fixed(
            start,
            end_excl,
            month=terms.balance_due_month,
            day=15,
            hour=10,
            minute=0,
        ):
            yield ObligationEvent(
                person_id=person_id,
                direction=Direction.OUTFLOW,
                counterparty_acct=terms.treasury_acct,
                amount=terms.balance_due_amount,
                timestamp=ts,
                channel=TAX_BALANCE_DUE,
                product_type="tax",
            )
