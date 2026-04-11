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

from common.channels import TAX_BALANCE_DUE, TAX_ESTIMATED_PAYMENT, TAX_REFUND
from common.persona_names import FREELANCER, HNW, RETIRED, SALARIED, SMALLBIZ, STUDENT
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
    """Per-person visible tax cash-flow parameters."""

    treasury_acct: str

    # Estimated tax profile (visible quarterly outflows)
    quarterly_amount: float = 0.0

    # Annual settlement profile (one of refund / balance due / none)
    refund_amount: float = 0.0
    refund_month: int = 3  # typical: Feb-May

    balance_due_amount: float = 0.0
    balance_due_month: int = 4  # typical filing deadline month

    def __post_init__(self) -> None:
        if not self.treasury_acct:
            raise ValueError("treasury_acct must be non-empty")

        ge("quarterly_amount", self.quarterly_amount, 0.0)
        ge("refund_amount", self.refund_amount, 0.0)
        ge("balance_due_amount", self.balance_due_amount, 0.0)

        between("refund_month", self.refund_month, 1, 12)
        between("balance_due_month", self.balance_due_month, 1, 12)

        if self.refund_amount > 0.0 and self.balance_due_amount > 0.0:
            raise ValueError(
                "annual settlement cannot be both a refund and a balance due"
            )


@dataclass(frozen=True, slots=True)
class TaxConfig:
    """Population-level parameters for estimated tax and annual settlement."""

    # --- estimated tax (quarterly visible outflows) ---
    # Anchor kept from your existing model.
    quarterly_median: float = 3500.0
    quarterly_sigma: float = 0.60

    # Persona-specific ownership of the estimated-tax profile only.
    rates: dict[str, float] | None = None

    # --- annual settlement (broad filing population) ---
    # Based on the broader filing population, not estimated-tax ownership.
    refund_p: float = 0.63
    refund_median: float = 3167.0
    refund_sigma: float = 0.45

    # Remaining filers can owe, or have ~zero visible settlement.
    balance_due_p: float = 0.27
    balance_due_median: float = 1450.0
    balance_due_sigma: float = 0.65

    def estimated_payment_p(self, persona: str) -> float:
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

    # Backward-compatible alias for any existing callers.
    def ownership_p(self, persona: str) -> float:
        return self.estimated_payment_p(persona)

    def __post_init__(self) -> None:
        gt("quarterly_median", self.quarterly_median, 0.0)
        ge("quarterly_sigma", self.quarterly_sigma, 0.0)

        between("refund_p", self.refund_p, 0.0, 1.0)
        gt("refund_median", self.refund_median, 0.0)
        ge("refund_sigma", self.refund_sigma, 0.0)

        between("balance_due_p", self.balance_due_p, 0.0, 1.0)
        gt("balance_due_median", self.balance_due_median, 0.0)
        ge("balance_due_sigma", self.balance_due_sigma, 0.0)

        if float(self.refund_p) + float(self.balance_due_p) > 1.0:
            raise ValueError("refund_p + balance_due_p must be <= 1.0")


DEFAULT_TAX_CONFIG = TaxConfig()


def scheduled_events(
    person_id: str,
    terms: TaxTerms,
    start: datetime,
    end_excl: datetime,
) -> Iterator[ObligationEvent]:
    """Yield quarterly estimated-tax payments and annual settlement events."""
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
                        channel=TAX_ESTIMATED_PAYMENT,
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
                    channel=TAX_REFUND,
                    product_type="tax",
                )

    # Annual balance due
    if terms.balance_due_amount > 0.0:
        for year in range(start.year, end_excl.year + 1):
            ts = datetime(year, terms.balance_due_month, 15, 10, 0, 0)
            if ts >= start and ts < end_excl:
                yield ObligationEvent(
                    person_id=person_id,
                    direction=Direction.OUTFLOW,
                    counterparty_acct=terms.treasury_acct,
                    amount=terms.balance_due_amount,
                    timestamp=ts,
                    channel=TAX_BALANCE_DUE,
                    product_type="tax",
                )
