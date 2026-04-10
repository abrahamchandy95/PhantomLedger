from collections.abc import Iterator

from entities import models
from ..csv_io import Row


def account_number(data: models.Accounts) -> Iterator[Row]:
    """
    Yields rows for the ACCOUNT_NUMBER vertex table.
    (account_number, mule, fraud, victim, is_external)
    """
    mules = data.mules
    frauds = data.frauds
    victims = data.victims
    externals = data.externals

    for acc_id in data.ids:
        yield (
            acc_id,
            int(acc_id in mules),
            int(acc_id in frauds),
            int(acc_id in victims),
            int(acc_id in externals),
        )


def has_account(data: models.Accounts) -> Iterator[Row]:
    """
    Yields rows for the HAS_ACCOUNT edge table.
    (FROM, TO)
    """
    for person_id, account_ids in data.by_person.items():
        for acc_id in account_ids:
            yield (person_id, acc_id)
