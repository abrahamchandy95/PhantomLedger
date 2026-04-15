from dataclasses import dataclass, field
from common.validate import validate_metadata


@dataclass(frozen=True, slots=True)
class Events:
    clearing_accounts: int = field(default=3, metadata={"ge": 0})
    unknown_outflow_p: float = field(default=0.05, metadata={"between": (0.0, 1.0)})
    day_multiplier_shape: float = field(default=1.3, metadata={"gt": 0.0})

    max_per_person_per_day: int = field(default=0, metadata={"ge": 0})
    prefer_billers_p: float = field(default=0.55, metadata={"between": (0.0, 1.0)})

    def __post_init__(self) -> None:
        validate_metadata(self)
