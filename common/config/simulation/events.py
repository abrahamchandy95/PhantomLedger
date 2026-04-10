from dataclasses import dataclass

from common.validate import (
    between,
    gt,
    ge,
)


@dataclass(frozen=True, slots=True)
class Events:
    clearing_accounts: int = 3
    unknown_outflow_p: float = 0.45

    day_multiplier_shape: float = 1.3
    max_per_day: int = 4
    prefer_billers_p: float = 0.55

    def __post_init__(self) -> None:
        ge("clearing_accounts", self.clearing_accounts, 0)
        between("unknown_outflow_p", self.unknown_outflow_p, 0.0, 1.0)
        gt("day_multiplier_shape", self.day_multiplier_shape, 0.0)
        ge("max_per_day", self.max_per_day, 0)
        between("prefer_billers_p", self.prefer_billers_p, 0.0, 1.0)
