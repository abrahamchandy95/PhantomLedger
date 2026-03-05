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
        for _t in range(trials):
            if rng.coin(p):
                extra += 1
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
      - We mirror the original approach: each flagged person has at least their primary account flagged.
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
        person_accounts[pid] = []
        for _ in range(k):
            aid = account_ids[cursor]
            cursor += 1

            accounts.append(aid)
            person_accounts[pid].append(aid)
            acct_owner[aid] = pid

        # Primary account = first account
        primary = person_accounts[pid][0]
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


def build_account_rows(data: AccountsData) -> list[CsvRow]:
    """
    Build rows for accountnumber.csv:
      account_number, mule, fraud, victim
    """
    rows: list[CsvRow] = []
    for a in data.accounts:
        row: list[CsvCell] = [
            a,
            1 if a in data.mule_accounts else 0,
            1 if a in data.fraud_accounts else 0,
            1 if a in data.victim_accounts else 0,
        ]
        rows.append(row)
    return rows


def build_has_account_rows(data: AccountsData) -> list[CsvRow]:
    """
    Build rows for HAS_ACCOUNT.csv:
      FROM, TO
    """
    rows: list[CsvRow] = []
    for p, accts in data.person_accounts.items():
        for a in accts:
            row: list[CsvCell] = [p, a]
            rows.append(row)
    return rows
