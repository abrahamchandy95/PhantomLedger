from dataclasses import dataclass

from common.validate import gt


@dataclass(frozen=True, slots=True)
class Accounts:
    max_per_person: int = 3

    def __post_init__(self) -> None:
        gt("max_per_person", self.max_per_person, 0)
