from dataclasses import dataclass

import numpy as np

from common.math import lognormal_by_median
from common.random import Rng, SeedBank
from entities.accounts import AccountsData

from .models import CreditCardData, CreditCardIssuanceRequest, empty_credit_cards
from .policy import CreditIssuancePolicy


def _cc_account_id(i: int) -> str:
    return f"L{i:09d}"


@dataclass(frozen=True, slots=True)
class _IssuedCard:
    account_id: str
    owner: str
    apr: float
    limit: float
    cycle_day: int
    autopay_mode: int


def _sample_autopay_mode(
    policy: CreditIssuancePolicy,
    gen: np.random.Generator,
) -> int:
    u = float(gen.random())
    if u < float(policy.autopay_full_p):
        return 2
    if u < float(policy.autopay_full_p) + float(policy.autopay_min_p):
        return 1
    return 0


def _issue_card_for_person(
    policy: CreditIssuancePolicy,
    *,
    person_id: str,
    persona: str,
    card_number: int,
    gen: np.random.Generator,
) -> _IssuedCard | None:
    if float(gen.random()) >= float(policy.owner_probability(persona)):
        return None

    card = _cc_account_id(card_number)

    apr = lognormal_by_median(
        gen,
        median=float(policy.apr_median),
        sigma=float(policy.apr_sigma),
    )
    apr = max(float(policy.apr_min), min(float(policy.apr_max), float(apr)))

    limit = lognormal_by_median(
        gen,
        median=float(policy.limit_median(persona)),
        sigma=float(policy.limit_sigma),
    )
    limit = max(200.0, float(limit))

    cycle_day = int(
        gen.integers(int(policy.cycle_day_min), int(policy.cycle_day_max) + 1)
    )

    return _IssuedCard(
        account_id=card,
        owner=person_id,
        apr=float(apr),
        limit=float(limit),
        cycle_day=int(cycle_day),
        autopay_mode=_sample_autopay_mode(policy, gen),
    )


def _copy_account_state(
    accounts: AccountsData,
) -> tuple[list[str], dict[str, list[str]], dict[str, str]]:
    new_accounts = list(accounts.accounts)
    new_person_accounts: dict[str, list[str]] = {
        person_id: list(acct_ids)
        for person_id, acct_ids in accounts.person_accounts.items()
    }
    new_acct_owner = dict(accounts.acct_owner)
    return new_accounts, new_person_accounts, new_acct_owner


def attach_credit_cards(
    policy: CreditIssuancePolicy,
    rng: Rng,
    request: CreditCardIssuanceRequest,
) -> tuple[AccountsData, CreditCardData]:
    """
    Create credit card liability accounts (prefix L) and attach them to persons.

    Caller decides whether the feature is enabled.
    """
    policy.validate()

    persons = sorted(request.accounts.person_accounts)
    if not persons:
        return request.accounts, empty_credit_cards()

    seedbank = SeedBank(request.base_seed)

    new_accounts, new_person_accounts, new_acct_owner = _copy_account_state(
        request.accounts
    )

    card_accounts: list[str] = []
    card_for_person: dict[str, str] = {}
    owner_for_card: dict[str, str] = {}

    apr_by_card: dict[str, float] = {}
    limit_by_card: dict[str, float] = {}
    cycle_day_by_card: dict[str, int] = {}
    autopay_mode_by_card: dict[str, int] = {}

    issued_count = 0

    for person_id in persons:
        persona = request.persona_for_person.get(person_id, "salaried")
        gen = seedbank.generator("cc_issue", person_id)

        issued = _issue_card_for_person(
            policy,
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

    # Preserve global RNG advancement expectations.
    _ = rng.float()

    updated_accounts = AccountsData(
        accounts=new_accounts,
        person_accounts=new_person_accounts,
        acct_owner=new_acct_owner,
        fraud_accounts=request.accounts.fraud_accounts,
        mule_accounts=request.accounts.mule_accounts,
        victim_accounts=request.accounts.victim_accounts,
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
