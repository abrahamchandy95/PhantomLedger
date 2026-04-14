from dataclasses import dataclass

from common.validate import gt


@dataclass(frozen=True, slots=True)
class Population:
    seed: int = 7
    size: int = 200_000

    def __post_init__(self) -> None:
        gt("size", self.size, 0)
