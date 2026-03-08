from dataclasses import dataclass

from common.validate import require_float_between


@dataclass(frozen=True, slots=True)
class InfraConfig:
    infra_switch_p: float = 0.05

    def validate(self) -> None:
        require_float_between("infra_switch_p", self.infra_switch_p, 0.0, 1.0)
