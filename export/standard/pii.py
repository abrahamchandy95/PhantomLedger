from collections.abc import Iterator

from entities import models
from ..csv_io import Row


def phone(data: models.Pii) -> Iterator[Row]:
    """
    Yields rows for the PHONE vertex table.
    (phone_id,)
    """
    for phone_num in data.phone_map.values():
        yield (phone_num,)


def email(data: models.Pii) -> Iterator[Row]:
    """
    Yields rows for the EMAIL vertex table.
    (email_id,)
    """
    for email_addr in data.email_map.values():
        yield (email_addr,)


def has_phone(people: list[str], data: models.Pii) -> Iterator[Row]:
    """
    Yields rows for the HAS_PHONE edge table.
    (FROM, TO)
    """
    for person_id in people:
        yield (person_id, data.phone_map[person_id])


def has_email(people: list[str], data: models.Pii) -> Iterator[Row]:
    """
    Yields rows for the HAS_EMAIL edge table.
    (FROM, TO)
    """
    for person_id in people:
        yield (person_id, data.email_map[person_id])
