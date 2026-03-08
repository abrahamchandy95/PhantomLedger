from collections.abc import Iterator

from emit.csv_io import CsvCell, CsvRow
from entities.people import PeopleData


def iter_person_rows(data: PeopleData) -> Iterator[CsvRow]:
    """
    person.csv rows:
      customer_id, mule, fraud, victim
    """
    for person_id in data.people:
        row: list[CsvCell] = [
            person_id,
            1 if person_id in data.mule_people else 0,
            1 if person_id in data.fraud_people else 0,
            1 if person_id in data.victim_people else 0,
        ]
        yield row
