from dataclasses import dataclass

from common.validate import require_int_gt


@dataclass(frozen=True, slots=True)
class PopulationConfig:
    seed: int = 7
    size: int = 10_000

    def validate(self) -> None:
        require_int_gt("size", self.size, 0)
