from collections.abc import Iterator
from dataclasses import dataclass

import numpy as np

from common.math import lognormal_by_median
from common.random import RngFactory
from .. import models

from .policy import IssuancePolicy


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
    policy: IssuancePolicy,
    gen: np.random.Generator,
) -> int:
    u = gen.random()
    if u < policy.autopay_full_p:
        return 2
    if u < policy.autopay_full_p + policy.autopay_min_p:
        return 1
    return 0


def _issue_card(
    policy: IssuancePolicy,
    persona: models.Persona,
    card_id: str,
    person_id: str,
    gen: np.random.Generator,
) -> _IssuedCard:
    """Pure data factory. Assumes approval logic has already passed."""
    apr = lognormal_by_median(gen, median=policy.apr_median, sigma=policy.apr_sigma)
    apr = max(policy.apr_min, min(policy.apr_max, float(apr)))

    limit = lognormal_by_median(
        gen, median=persona.credit_limit, sigma=policy.limit_sigma
    )
    limit = max(200.0, float(limit))

    cycle_day = int(gen.integers(policy.cycle_day_min, policy.cycle_day_max + 1))

    return _IssuedCard(
        account_id=card_id,
        owner=person_id,
        apr=float(apr),
        limit=float(limit),
        cycle_day=cycle_day,
        autopay_mode=_sample_autopay_mode(policy, gen),
    )


def _process_applications(
    policy: IssuancePolicy,
    base_seed: int,
    persons: list[str],
    persona_map: dict[str, models.Persona],
) -> Iterator[_IssuedCard]:
    """Uses a generator (yield) to bypass memory overhead and .append() penalties."""
    rng_factory = RngFactory(base_seed)
    issued_count = 0

    for person_id in persons:
        persona = persona_map[person_id]
        gen = rng_factory.rng("cc_issue", person_id).gen

        # Check approval immediately to save compute
        if gen.random() >= persona.card_prob:
            continue

        issued_count += 1
        card_id = _cc_account_id(issued_count)

        # Yield directly instead of appending
        yield _issue_card(policy, persona, card_id, person_id, gen)


def build(
    policy: IssuancePolicy,
    base_seed: int,
    accounts: models.Accounts,
    persona_map: dict[str, models.Persona],
) -> tuple[models.Accounts, models.CreditCards]:
    """Orchestrates generation and uses C-speed merges to update state."""
    persons = sorted(accounts.by_person.keys())
    if not persons:
        return accounts, models.CreditCards([], {}, {}, {}, {}, {}, {})

    # 1. Consume the generator into a list exactly once
    issued = list(_process_applications(policy, base_seed, persons, persona_map))

    # 2. Extract arrays using blisteringly fast C-speed list/dict comprehensions
    card_ids = [c.account_id for c in issued]
    owner_map = {c.account_id: c.owner for c in issued}

    # 3. Create the CreditCards struct
    cards = models.CreditCards(
        ids=card_ids,
        by_person={c.owner: c.account_id for c in issued},
        owner_map=owner_map,
        aprs={c.account_id: c.apr for c in issued},
        limits={c.account_id: c.limit for c in issued},
        cycle_days={c.account_id: c.cycle_day for c in issued},
        autopay_modes={c.account_id: c.autopay_mode for c in issued},
    )

    # 4. Safely update the Accounts struct
    # (accounts.ids + card_ids natively concatenates lists in C)
    # (dict1 | dict2 natively merges dictionaries in Python 3.9+)
    new_by_person = {
        pid: list(acct_ids) for pid, acct_ids in accounts.by_person.items()
    }
    for card in issued:
        new_by_person.setdefault(card.owner, []).append(card.account_id)

    updated_accounts = models.Accounts(
        ids=accounts.ids + card_ids,  # Highly optimized merge
        by_person=new_by_person,
        owner_map=accounts.owner_map | owner_map,  # Highly optimized merge
        frauds=accounts.frauds,
        mules=accounts.mules,
        victims=accounts.victims,
        externals=accounts.externals,
    )

    return updated_accounts, cards
