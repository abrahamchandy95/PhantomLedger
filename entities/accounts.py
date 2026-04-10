from typing import cast

from common import config
from common.ids import accounts
from common.random import Rng
from . import models


def _accounts_per_person(cfg: config.Accounts, rng: Rng, n: int) -> list[int]:
    """
    Draw number of accounts per person.
    Vectorized via NumPy for maximum speed while satisfying strict Pyright typing.
    """
    max_per = int(cfg.max_per_person)
    if max_per <= 1:
        return [1] * n

    trials = max_per - 1

    raw_draws = rng.gen.binomial(n=trials, p=0.25, size=n) + 1

    return cast(list[int], raw_draws.tolist())


def build(cfg: config.Accounts, rng: Rng, people_mdl: models.People) -> models.Accounts:
    """
    Generate account IDs and mappings.
    """
    persons = people_mdl.ids
    counts = _accounts_per_person(cfg, rng, len(persons))

    total_accounts = sum(counts)
    account_ids = list(accounts(total_accounts))

    by_person: dict[str, list[str]] = {}
    owner_map: dict[str, str] = {}

    frauds: set[str] = set()
    mules: set[str] = set()
    victims: set[str] = set()

    cursor = 0
    for pid, k in zip(persons, counts, strict=True):
        accts = account_ids[cursor : cursor + k]
        cursor += k

        by_person[pid] = accts

        for aid in accts:
            owner_map[aid] = pid

    frauds = {by_person[pid][0] for pid in people_mdl.frauds}
    mules = {by_person[pid][0] for pid in people_mdl.mules}
    victims = {by_person[pid][0] for pid in people_mdl.victims}

    return models.Accounts(
        ids=account_ids,
        by_person=by_person,
        owner_map=owner_map,
        frauds=frauds,
        mules=mules,
        victims=victims,
        externals=set(),
    )


def merge(
    data: models.Accounts,
    new_ids: list[str],
    *,
    mark_external: bool = False,
) -> models.Accounts:
    """
    Safely merges new accounts into the existing Accounts pool.
    """
    if not new_ids:
        return data

    existing = set(data.ids)
    additions = [a for a in new_ids if a not in existing]

    if not additions:
        return data

    new_list = data.ids + additions
    new_externals = set(data.externals)

    if mark_external:
        new_externals.update(additions)

    return models.Accounts(
        ids=new_list,
        by_person=data.by_person,
        owner_map=data.owner_map,
        frauds=data.frauds,
        mules=data.mules,
        victims=data.victims,
        externals=new_externals,
    )
