from collections.abc import Iterator

from entities import models
from ..csv_io import Row


def person(data: models.People) -> Iterator[Row]:
    """
    Yields rows for the PERSON vertex table.
    (customer_id, mule, fraud, victim, solo_fraud)
    """
    mules = data.mules
    frauds = data.frauds
    victims = data.victims
    solos = data.solo_frauds

    for person_id in data.ids:
        yield (
            person_id,
            int(person_id in mules),
            int(person_id in frauds),
            int(person_id in victims),
            int(person_id in solos),
        )
