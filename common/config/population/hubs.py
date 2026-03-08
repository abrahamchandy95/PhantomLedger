from dataclasses import dataclass

from common.validate import require_float_between


@dataclass(frozen=True, slots=True)
class HubsConfig:
    hub_fraction: float = 0.01

    def validate(self) -> None:
        require_float_between("hub_fraction", self.hub_fraction, 0.0, 0.5)
