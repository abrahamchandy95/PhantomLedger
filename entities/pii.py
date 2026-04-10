from common.random import Rng
from . import models


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


def generate(people: models.People, rng: Rng) -> models.Pii:
    """
    Generate phone/email for each person.
    """
    persons = people.ids

    phone_map = {p: _rand_us_phone(rng) for p in persons}
    email_map = {p: _email_for_person(p) for p in persons}

    return models.Pii(phone_map=phone_map, email_map=email_map)
