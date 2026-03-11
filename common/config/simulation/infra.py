from dataclasses import dataclass

from common.validate import require_float_between


@dataclass(frozen=True, slots=True)
class InfraConfig:
    switch_prob: float = 0.05

    def validate(self) -> None:
        require_float_between("switch_prob", self.switch_prob, 0.0, 1.0)
