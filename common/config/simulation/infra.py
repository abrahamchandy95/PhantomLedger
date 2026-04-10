from dataclasses import dataclass

from common.validate import between


@dataclass(frozen=True, slots=True)
class Infra:
    switch_prob: float = 0.05

    def __post_init__(self) -> None:
        between("switch_prob", self.switch_prob, 0.0, 1.0)
