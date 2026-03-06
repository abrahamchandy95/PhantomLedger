from collections.abc import Iterator
from dataclasses import dataclass

from common.config import AccountsConfig
from common.ids import iter_account_ids
from common.rng import Rng
from emit.tg_csv import CsvCell, CsvRow

from entities.people import PeopleData


@dataclass(frozen=True, slots=True)
class AccountsData:
    accounts: list[str]
    person_accounts: dict[str, list[str]]
    acct_owner: dict[str, str]

    fraud_accounts: set[str]
    mule_accounts: set[str]
    victim_accounts: set[str]


def _account_count_per_person(acfg: AccountsConfig, rng: Rng, n: int) -> list[int]:
    """
    Draw number of accounts per person.

    Mirrors your original distribution:
      1 + Binomial(n=max_accounts_per_person-1, p=0.25), clipped.

    Implemented as explicit Bernoulli trials to avoid NumPy typing issues
    under strict basedpyright settings.
    """
    max_per = int(acfg.max_accounts_per_person)
    if max_per <= 1:
        return [1] * n

    trials = max_per - 1
    p = 0.25
    max_k = max_per

    out: list[int] = []
    for _ in range(n):
        extra = 0
        for _ in range(trials):
            extra += 1 if rng.coin(p) else 0

        k = 1 + extra
        if k < 1:
            k = 1
        elif k > max_k:
            k = max_k

        out.append(k)

    return out


def generate_accounts(
    acfg: AccountsConfig, rng: Rng, people: PeopleData
) -> AccountsData:
    """
    Generate account IDs and mappings.

    Flags:
      - Each flagged person has at least their primary account flagged.
      - If a person has >1 accounts, you can later decide to spread flags across multiple accounts,
        but we keep it simple and consistent with your original code.
    """
    persons = people.people
    counts = _account_count_per_person(acfg, rng, len(persons))

    total_accounts = sum(counts)
    account_ids = list(iter_account_ids(total_accounts))

    accounts: list[str] = []
    person_accounts: dict[str, list[str]] = {}
    acct_owner: dict[str, str] = {}

    fraud_accounts: set[str] = set()
    mule_accounts: set[str] = set()
    victim_accounts: set[str] = set()

    cursor = 0
    for pid, k in zip(persons, counts):
        accts: list[str] = []
        for _ in range(k):
            aid = account_ids[cursor]
            cursor += 1
            accounts.append(aid)
            accts.append(aid)
            acct_owner[aid] = pid

        person_accounts[pid] = accts

        primary = accts[0]
        if pid in people.fraud_people:
            fraud_accounts.add(primary)
        if pid in people.mule_people:
            mule_accounts.add(primary)
        if pid in people.victim_people:
            victim_accounts.add(primary)

    return AccountsData(
        accounts=accounts,
        person_accounts=person_accounts,
        acct_owner=acct_owner,
        fraud_accounts=fraud_accounts,
        mule_accounts=mule_accounts,
        victim_accounts=victim_accounts,
    )


def iter_account_rows(data: AccountsData) -> Iterator[CsvRow]:
    """
    accountnumber.csv rows:
      account_number, mule, fraud, victim
    """
    for a in data.accounts:
        row: list[CsvCell] = [
            a,
            1 if a in data.mule_accounts else 0,
            1 if a in data.fraud_accounts else 0,
            1 if a in data.victim_accounts else 0,
        ]
        yield row


def iter_has_account_rows(data: AccountsData) -> Iterator[CsvRow]:
    """
    HAS_ACCOUNT.csv rows:
      FROM, TO
    """
    for p, accts in data.person_accounts.items():
        for a in accts:
            row: list[CsvCell] = [p, a]
            yield row


def with_extra_accounts(data: AccountsData, extra_accounts: list[str]) -> AccountsData:
    """
    Return a new AccountsData with extra accounts appended to .accounts.

    Does NOT assign owners (acct_owner unchanged).
    Keeps all flag sets unchanged.
    """
    if not extra_accounts:
        return data

    existing = set(data.accounts)
    new_list = list(data.accounts)
    for a in extra_accounts:
        if a not in existing:
            new_list.append(a)
            existing.add(a)

    return AccountsData(
        accounts=new_list,
        person_accounts=data.person_accounts,
        acct_owner=data.acct_owner,
        fraud_accounts=data.fraud_accounts,
        mule_accounts=data.mule_accounts,
        victim_accounts=data.victim_accounts,
    )
