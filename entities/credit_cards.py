from dataclasses import dataclass

import numpy as np

from common.config import CreditConfig
from common.probability import lognormal_by_median
from common.rng import Rng
from common.seeding import derived_seed
from entities.accounts import AccountsData


def _cc_account_id(i: int) -> str:
    return f"L{i:09d}"


@dataclass(frozen=True, slots=True)
class CreditCardData:
    card_accounts: list[str]
    card_for_person: dict[str, str]  # person -> L...
    owner_for_card: dict[str, str]  # L... -> person

    apr_by_card: dict[str, float]
    limit_by_card: dict[str, float]
    cycle_day_by_card: dict[str, int]
    autopay_mode_by_card: dict[str, int]  # 0 none, 1 min, 2 full


@dataclass(frozen=True, slots=True)
class _IssuedCard:
    account_id: str
    owner: str
    apr: float
    limit: float
    cycle_day: int
    autopay_mode: int


def _empty_credit_cards() -> CreditCardData:
    return CreditCardData([], {}, {}, {}, {}, {}, {})


def _sample_autopay_mode(ccfg: CreditConfig, gen: np.random.Generator) -> int:
    u = float(gen.random())
    if u < float(ccfg.autopay_full_p):
        return 2
    if u < float(ccfg.autopay_full_p) + float(ccfg.autopay_min_p):
        return 1
    return 0


def _issue_card_for_person(
    ccfg: CreditConfig,
    *,
    person_id: str,
    persona: str,
    card_number: int,
    gen: np.random.Generator,
) -> _IssuedCard | None:
    if float(gen.random()) >= float(ccfg.owner_p(persona)):
        return None

    card = _cc_account_id(card_number)

    apr = lognormal_by_median(
        gen,
        median=float(ccfg.apr_median),
        sigma=float(ccfg.apr_sigma),
    )
    apr = max(float(ccfg.apr_min), min(float(ccfg.apr_max), float(apr)))

    limit = lognormal_by_median(
        gen,
        median=float(ccfg.limit_median(persona)),
        sigma=float(ccfg.limit_sigma),
    )
    limit = max(200.0, float(limit))

    cycle_day = int(gen.integers(int(ccfg.cycle_day_min), int(ccfg.cycle_day_max) + 1))

    return _IssuedCard(
        account_id=card,
        owner=person_id,
        apr=float(apr),
        limit=float(limit),
        cycle_day=int(cycle_day),
        autopay_mode=_sample_autopay_mode(ccfg, gen),
    )


def attach_credit_cards(
    ccfg: CreditConfig,
    rng: Rng,
    *,
    base_seed: int,
    accounts: AccountsData,
    persons: list[str],
    persona_for_person: dict[str, str],
) -> tuple[AccountsData, CreditCardData]:
    """
    Create credit card liability accounts (prefix L) and attach them to persons.

    Returns a NEW AccountsData with updated:
      - accounts
      - person_accounts
      - acct_owner

    Fraud / mule / victim flags remain unchanged.
    """
    if not ccfg.enable_credit_cards or not persons:
        return accounts, _empty_credit_cards()

    sorted_persons = sorted(persons)

    new_accounts = list(accounts.accounts)
    new_person_accounts: dict[str, list[str]] = {
        p: list(accts) for p, accts in accounts.person_accounts.items()
    }
    new_acct_owner = dict(accounts.acct_owner)

    card_accounts: list[str] = []
    card_for_person: dict[str, str] = {}
    owner_for_card: dict[str, str] = {}

    apr_by_card: dict[str, float] = {}
    limit_by_card: dict[str, float] = {}
    cycle_day_by_card: dict[str, int] = {}
    autopay_mode_by_card: dict[str, int] = {}

    issued_count = 0

    for person_id in sorted_persons:
        persona = persona_for_person.get(person_id, "salaried")
        gen = np.random.default_rng(derived_seed(base_seed, "cc_issue", person_id))

        issued = _issue_card_for_person(
            ccfg,
            person_id=person_id,
            persona=persona,
            card_number=issued_count + 1,
            gen=gen,
        )
        if issued is None:
            continue

        issued_count += 1

        new_accounts.append(issued.account_id)
        new_person_accounts.setdefault(person_id, []).append(issued.account_id)
        new_acct_owner[issued.account_id] = person_id

        card_accounts.append(issued.account_id)
        card_for_person[person_id] = issued.account_id
        owner_for_card[issued.account_id] = person_id
        apr_by_card[issued.account_id] = issued.apr
        limit_by_card[issued.account_id] = issued.limit
        cycle_day_by_card[issued.account_id] = issued.cycle_day
        autopay_mode_by_card[issued.account_id] = issued.autopay_mode

    # Preserve existing global RNG advancement expectations.
    _ = rng.float()

    updated_accounts = AccountsData(
        accounts=new_accounts,
        person_accounts=new_person_accounts,
        acct_owner=new_acct_owner,
        fraud_accounts=accounts.fraud_accounts,
        mule_accounts=accounts.mule_accounts,
        victim_accounts=accounts.victim_accounts,
    )

    cards = CreditCardData(
        card_accounts=card_accounts,
        card_for_person=card_for_person,
        owner_for_card=owner_for_card,
        apr_by_card=apr_by_card,
        limit_by_card=limit_by_card,
        cycle_day_by_card=cycle_day_by_card,
        autopay_mode_by_card=autopay_mode_by_card,
    )

    return updated_accounts, cards
