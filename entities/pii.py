from dataclasses import dataclass

from common.rng import Rng
from emit.tg_csv import CsvCell, CsvRow


@dataclass(frozen=True, slots=True)
class PiiData:
    person_phone: dict[str, str]
    person_email: dict[str, str]


def _rand_us_phone(rng: Rng) -> str:
    """
    Generate a US-looking E.164 phone string.
    Avoids obvious invalid NANP area codes starting with 0/1.
    """
    area = rng.int(200, 999)
    exch = rng.int(200, 999)
    line = rng.int(0, 10_000)
    return f"+1{area:03d}{exch:03d}{line:04d}"


def _email_for_person(customer_id: str) -> str:
    return f"user{customer_id.lower()}@example.com"


def generate_pii(people: list[str], rng: Rng) -> PiiData:
    """
    Generate phone/email for each person.
    """
    person_phone: dict[str, str] = {}
    person_email: dict[str, str] = {}

    for p in people:
        person_phone[p] = _rand_us_phone(rng)
        person_email[p] = _email_for_person(p)

    return PiiData(person_phone=person_phone, person_email=person_email)


def build_phone_rows(pii: PiiData) -> list[CsvRow]:
    """phone.csv rows: phone_id"""
    rows: list[CsvRow] = []
    for phone in pii.person_phone.values():
        row: list[CsvCell] = [phone]
        rows.append(row)
    return rows


def build_email_rows(pii: PiiData) -> list[CsvRow]:
    """email.csv rows: email_id"""
    rows: list[CsvRow] = []
    for email in pii.person_email.values():
        row: list[CsvCell] = [email]
        rows.append(row)
    return rows


def build_has_phone_rows(people: list[str], pii: PiiData) -> list[CsvRow]:
    """HAS_PHONE.csv rows: FROM, TO"""
    rows: list[CsvRow] = []
    for p in people:
        row: list[CsvCell] = [p, pii.person_phone[p]]
        rows.append(row)
    return rows


def build_has_email_rows(people: list[str], pii: PiiData) -> list[CsvRow]:
    """HAS_EMAIL.csv rows: FROM, TO"""
    rows: list[CsvRow] = []
    for p in people:
        row: list[CsvCell] = [p, pii.person_email[p]]
        rows.append(row)
    return rows
