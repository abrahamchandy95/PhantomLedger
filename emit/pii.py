from collections.abc import Iterator

from emit.csv_io import CsvCell, CsvRow
from entities.pii import PiiData


def iter_phone_rows(pii: PiiData) -> Iterator[CsvRow]:
    """phone.csv rows: phone_id"""
    for phone in pii.person_phone.values():
        row: list[CsvCell] = [phone]
        yield row


def iter_email_rows(pii: PiiData) -> Iterator[CsvRow]:
    """email.csv rows: email_id"""
    for email in pii.person_email.values():
        row: list[CsvCell] = [email]
        yield row


def iter_has_phone_rows(people: list[str], pii: PiiData) -> Iterator[CsvRow]:
    """HAS_PHONE.csv rows: FROM, TO"""
    for person_id in people:
        row: list[CsvCell] = [person_id, pii.person_phone[person_id]]
        yield row


def iter_has_email_rows(people: list[str], pii: PiiData) -> Iterator[CsvRow]:
    """HAS_EMAIL.csv rows: FROM, TO"""
    for person_id in people:
        row: list[CsvCell] = [person_id, pii.person_email[person_id]]
        yield row
