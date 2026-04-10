"""
Credit card product adapter.

Wraps the existing entities/credit_cards issuance system as a
product in the portfolio. The credit card is unique among products
because:

1. It creates a NEW account (the card account) rather than just
   generating transactions against an existing account.
2. Its transaction generation is reactive — it reads existing
   card_purchase transactions to compute interest, fees, payments.
3. It has its own balance ledger (credit limit, not deposit balance).

This adapter consolidates per-person card parameters that were
previously scattered across CreditCards.aprs, .limits, .cycle_days,
and .autopay_modes into a single typed record. The actual issuance
and lifecycle generation still lives in entities/credit_cards/ and
transfers/credit_cards/ respectively.

Statistics:
- 84% of US adults have at least one credit card (Fed 2023)
- Average cards per holder: 3.9 (Experian 2024)
- Average credit limit: $29,855 (Experian 2024)
- Average APR: 21.5% (Fed G.19, Q4 2024)
- Average balance carried: $6,580 (TransUnion Q3 2024)
"""

from dataclasses import dataclass

from common.validate import between, gt
from entities.models import CreditCards


@dataclass(frozen=True, slots=True)
class CardTerms:
    """Per-person credit card parameters.

    Consolidates fields that issuance.py stores across multiple
    dicts in the CreditCards model into a single typed record
    that the portfolio can reference.
    """

    card_account_id: str
    apr: float
    credit_limit: float
    cycle_day: int
    autopay_mode: int  # 0=manual, 1=min, 2=full

    def __post_init__(self) -> None:
        gt("apr", self.apr, 0.0)
        gt("credit_limit", self.credit_limit, 0.0)
        between("cycle_day", self.cycle_day, 1, 28)
        between("autopay_mode", self.autopay_mode, 0, 2)


def from_existing(
    card_id: str,
    cards: CreditCards,
) -> CardTerms | None:
    """
    Build CardTerms from the existing CreditCards model.

    This is the bridge function: called during portfolio assembly
    to pull scattered card data into the product abstraction.

    Returns None if any required field is missing for this card.
    """
    apr = cards.aprs.get(card_id)
    limit = cards.limits.get(card_id)
    cycle_day = cards.cycle_days.get(card_id)
    autopay = cards.autopay_modes.get(card_id)

    if apr is None or limit is None or cycle_day is None or autopay is None:
        return None

    return CardTerms(
        card_account_id=card_id,
        apr=apr,
        credit_limit=limit,
        cycle_day=cycle_day,
        autopay_mode=autopay,
    )


def extract_all(cards: CreditCards) -> dict[str, CardTerms]:
    """
    Extract CardTerms for every issued card.

    Returns a dict keyed by person_id (not card_id), since the
    portfolio is organized per-person and each person has at most
    one card in the current model.
    """
    result: dict[str, CardTerms] = {}

    for person_id, card_id in cards.by_person.items():
        terms = from_existing(card_id, cards)
        if terms is not None:
            result[person_id] = terms

    return result
