from collections.abc import Iterator

from emit.csv_io import CsvCell, CsvRow
from entities.accounts import AccountsData


def iter_account_rows(data: AccountsData) -> Iterator[CsvRow]:
    """
    accountnumber.csv rows:
      account_number, mule, fraud, victim
    """
    for account_id in data.accounts:
        row: list[CsvCell] = [
            account_id,
            1 if account_id in data.mule_accounts else 0,
            1 if account_id in data.fraud_accounts else 0,
            1 if account_id in data.victim_accounts else 0,
        ]
        yield row


def iter_has_account_rows(data: AccountsData) -> Iterator[CsvRow]:
    """
    HAS_ACCOUNT.csv rows:
      FROM, TO
    """
    for person_id, account_ids in data.person_accounts.items():
        for account_id in account_ids:
            row: list[CsvCell] = [person_id, account_id]
            yield row
