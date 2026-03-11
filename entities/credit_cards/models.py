from dataclasses import dataclass

from entities.accounts import AccountsData


@dataclass(frozen=True, slots=True)
class CreditCardIssuanceRequest:
    base_seed: int
    accounts: AccountsData
    persona_for_person: dict[str, str]


@dataclass(frozen=True, slots=True)
class CreditCardData:
    card_accounts: list[str]
    card_for_person: dict[str, str]  # person -> L...
    owner_for_card: dict[str, str]  # L... -> person

    apr_by_card: dict[str, float]
    limit_by_card: dict[str, float]
    cycle_day_by_card: dict[str, int]
    autopay_mode_by_card: dict[str, int]  # 0 none, 1 min, 2 full


def empty_credit_cards() -> CreditCardData:
    return CreditCardData([], {}, {}, {}, {}, {}, {})
