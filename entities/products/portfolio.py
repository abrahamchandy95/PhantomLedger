"""
Financial product portfolio — the complete set of obligations a person holds.
"""

from collections.abc import Iterator
from dataclasses import dataclass, field
from datetime import datetime

from common.calendar_cache import WindowCalendar

from .auto_loan import AutoLoanTerms, scheduled_events as auto_loan_events
from .card_account import CardTerms
from .event import ObligationEvent
from .insurance import InsuranceHoldings
from .mortgage import MortgageTerms, scheduled_events as mortgage_events
from .student_loan import StudentLoanTerms, scheduled_events as student_loan_events
from .tax_profile import TaxTerms, scheduled_events as tax_events


@dataclass(frozen=True, slots=True)
class Portfolio:
    """All financial products held by a single person."""

    person_id: str

    mortgage: MortgageTerms | None = None
    auto_loan: AutoLoanTerms | None = None
    student_loan: StudentLoanTerms | None = None
    tax: TaxTerms | None = None
    credit_card: CardTerms | None = None
    insurance: InsuranceHoldings | None = None

    def has_credit_card(self) -> bool:
        return self.credit_card is not None

    def has_insurance(self) -> bool:
        return self.insurance is not None and self.insurance.has_any()

    def is_homeowner(self) -> bool:
        if self.mortgage is not None:
            return True
        return bool(self.insurance is not None and self.insurance.home is not None)

    def has_vehicle(self) -> bool:
        if self.auto_loan is not None:
            return True
        return bool(self.insurance is not None and self.insurance.auto is not None)

    def scheduled_events(
        self,
        start: datetime,
        end_excl: datetime,
        *,
        calendar: WindowCalendar | None = None,
    ) -> Iterator[ObligationEvent]:
        """
        Yield all obligation events across all held products.

        Credit card lifecycle events are NOT emitted here —
        they are reactive (depend on existing card_purchase txns)
        and handled by transfers/credit_cards/generator.py.

        Insurance events are NOT emitted here either —
        premiums and claims are handled by transfers/insurance.py
        which has existing stochastic claim generation logic.
        """
        if calendar is None:
            calendar = WindowCalendar(start, end_excl)

        if self.mortgage is not None:
            yield from mortgage_events(
                self.person_id,
                self.mortgage,
                start,
                end_excl,
                calendar=calendar,
            )

        if self.auto_loan is not None:
            yield from auto_loan_events(
                self.person_id,
                self.auto_loan,
                start,
                end_excl,
                calendar=calendar,
            )

        if self.student_loan is not None:
            yield from student_loan_events(
                self.person_id,
                self.student_loan,
                start,
                end_excl,
                calendar=calendar,
            )

        if self.tax is not None:
            yield from tax_events(
                self.person_id,
                self.tax,
                start,
                end_excl,
                calendar=calendar,
            )


@dataclass(frozen=True, slots=True)
class PortfolioRegistry:
    """Maps person_id -> Portfolio for the entire population."""

    by_person: dict[str, Portfolio] = field(default_factory=dict)

    def get(self, person_id: str) -> Portfolio | None:
        return self.by_person.get(person_id)

    def all_events(
        self,
        start: datetime,
        end_excl: datetime,
    ) -> Iterator[ObligationEvent]:
        """Stream every obligation event across all people and products."""
        calendar = WindowCalendar(start, end_excl)
        for portfolio in self.by_person.values():
            yield from portfolio.scheduled_events(
                start,
                end_excl,
                calendar=calendar,
            )

    @property
    def card_holders(self) -> dict[str, str]:
        """Returns person_id -> card_account_id for CC payment routing."""
        return {
            pid: p.credit_card.card_account_id
            for pid, p in self.by_person.items()
            if p.credit_card is not None
        }
