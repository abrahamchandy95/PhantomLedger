from dataclasses import dataclass
from math import log
from typing import cast

import numpy as np

from common.config import CreditConfig
from common.rng import Rng
from common.seeding import derived_seed
from entities.accounts import AccountsData


_NumScalar = float | int | np.floating | np.integer


def _as_float(x: object) -> float:
    return float(cast(_NumScalar, x))


def _cc_account_id(i: int) -> str:
    # L000000001, ...
    return f"L{i:09d}"


def _lognormal_by_median(
    gen: np.random.Generator, median: float, sigma: float
) -> float:
    mu = log(max(1e-12, float(median)))
    x_obj: object = cast(object, gen.lognormal(mean=mu, sigma=float(sigma)))
    return _as_float(x_obj)


@dataclass(frozen=True, slots=True)
class CreditCardData:
    card_accounts: list[str]
    card_for_person: dict[str, str]  # person -> L...
    owner_for_card: dict[str, str]  # L... -> person

    apr_by_card: dict[str, float]
    limit_by_card: dict[str, float]
    cycle_day_by_card: dict[str, int]
    autopay_mode_by_card: dict[str, int]  # 0 none, 1 min, 2 full


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
    Creates credit card liability accounts (prefix L) and attaches them to persons.

    NOTE: We return a NEW AccountsData with updated:
      - accounts list
      - person_accounts mapping (card appended)
      - acct_owner mapping (owner set for card)

    All fraud/mule/victim flags are unchanged.
    """
    if not ccfg.enable_credit_cards or not persons:
        empty = CreditCardData([], {}, {}, {}, {}, {}, {})
        return accounts, empty

    # deterministic per-person decision (stable regardless of iteration order)
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

    issued = 0

    for p in sorted_persons:
        persona = persona_for_person.get(p, "salaried")
        g = np.random.default_rng(derived_seed(base_seed, "cc_issue", p))

        if float(g.random()) >= float(ccfg.owner_p(persona)):
            continue

        issued += 1
        card = _cc_account_id(issued)

        # APR + limit as lognormal-by-median
        apr = _lognormal_by_median(g, float(ccfg.apr_median), float(ccfg.apr_sigma))
        apr = max(float(ccfg.apr_min), min(float(ccfg.apr_max), float(apr)))

        lim_med = float(ccfg.limit_median(persona))
        limit = _lognormal_by_median(g, lim_med, float(ccfg.limit_sigma))
        limit = max(200.0, float(limit))

        cycle_day = int(
            g.integers(int(ccfg.cycle_day_min), int(ccfg.cycle_day_max) + 1)
        )

        # autopay mode (per card)
        u = float(g.random())
        if u < float(ccfg.autopay_full_p):
            autopay = 2
        elif u < float(ccfg.autopay_full_p) + float(ccfg.autopay_min_p):
            autopay = 1
        else:
            autopay = 0

        # attach to accounts
        new_accounts.append(card)
        new_person_accounts.setdefault(p, []).append(card)
        new_acct_owner[card] = p

        card_accounts.append(card)
        card_for_person[p] = card
        owner_for_card[card] = p
        apr_by_card[card] = float(apr)
        limit_by_card[card] = float(limit)
        cycle_day_by_card[card] = int(cycle_day)
        autopay_mode_by_card[card] = int(autopay)

    # touch global rng once to preserve existing reproducibility expectations
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
