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
    Safely merges new unowned accounts into the existing Accounts pool.
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


def merge_owned_accounts(
    data: models.Accounts,
    owned_accounts: dict[str, list[str]],
    *,
    mark_external: bool = False,
) -> models.Accounts:
    """
    Merge accounts that should be represented and attributed to known people.

    This is used for modeled external accounts that still belong to a known person,
    such as a spouse/child/sibling who banks elsewhere. Those accounts must exist in
    the global registry and remain attributable for routing and graph export.
    """
    if not owned_accounts:
        return data

    ids = list(data.ids)
    known_ids = set(ids)

    by_person = {person_id: list(accts) for person_id, accts in data.by_person.items()}
    owner_map = dict(data.owner_map)
    externals = set(data.externals)

    changed = False

    for person_id, account_ids in owned_accounts.items():
        if not account_ids:
            continue

        person_accounts = by_person.setdefault(person_id, [])

        for account_id in account_ids:
            existing_owner = owner_map.get(account_id)
            if existing_owner is not None and existing_owner != person_id:
                raise ValueError(
                    f"Account {account_id!r} already belongs to {existing_owner!r}, "
                    + f"cannot reassign to {person_id!r}"
                )

            if account_id not in known_ids:
                ids.append(account_id)
                known_ids.add(account_id)
                changed = True

            if existing_owner is None:
                owner_map[account_id] = person_id
                changed = True

            if account_id not in person_accounts:
                person_accounts.append(account_id)
                changed = True

            if mark_external and account_id not in externals:
                externals.add(account_id)
                changed = True

    if not changed:
        return data

    return models.Accounts(
        ids=ids,
        by_person=by_person,
        owner_map=owner_map,
        frauds=data.frauds,
        mules=data.mules,
        victims=data.victims,
        externals=externals,
    )
