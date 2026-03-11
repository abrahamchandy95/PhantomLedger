from dataclasses import dataclass

from common.validate import (
    require_float_between,
    require_float_gt,
    require_int_ge,
)


@dataclass(frozen=True, slots=True)
class EventsConfig:
    num_clearing_accounts: int = 3
    unknown_outflow_p: float = 0.45

    day_multiplier_gamma_shape: float = 1.3
    max_events_per_day: int = 0
    prefer_billers_p: float = 0.55

    def validate(self) -> None:
        require_int_ge("num_clearing_accounts", self.num_clearing_accounts, 0)
        require_float_between("unknown_outflow_p", self.unknown_outflow_p, 0.0, 1.0)
        require_float_gt(
            "day_multiplier_gamma_shape",
            self.day_multiplier_gamma_shape,
            0.0,
        )
        require_int_ge("max_events_per_day", self.max_events_per_day, 0)
        require_float_between("prefer_billers_p", self.prefer_billers_p, 0.0, 1.0)
