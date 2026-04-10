from dataclasses import dataclass

from common.validate import between


@dataclass(frozen=True, slots=True)
class Hubs:
    fraction: float = 0.01

    def __post_init__(self) -> None:
        between("fraction", self.fraction, 0.0, 0.5)
