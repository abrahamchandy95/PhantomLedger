from dataclasses import dataclass, field

from common.validate import ge, validate_metadata


@dataclass(frozen=True, slots=True)
class CounterpartyRevenueProfile:
    active_p: float = field(default=0.0, metadata={"between": (0.0, 1.0)})
    counterparties_min: int = field(default=1, metadata={"ge": 1})
    counterparties_max: int = 1
    payments_min: int = field(default=0, metadata={"ge": 0})
    payments_max: int = 0
    median: float = field(default=0.0, metadata={"ge": 0.0})
    sigma: float = field(default=0.0, metadata={"ge": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)
        ge("counterparties_max", self.counterparties_max, self.counterparties_min)
        ge("payments_max", self.payments_max, self.payments_min)


@dataclass(frozen=True, slots=True)
class RevenueFlowProfile:
    active_p: float = field(default=0.0, metadata={"between": (0.0, 1.0)})
    payments_min: int = field(default=0, metadata={"ge": 0})
    payments_max: int = 0
    median: float = field(default=0.0, metadata={"ge": 0.0})
    sigma: float = field(default=0.0, metadata={"ge": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)
        ge("payments_max", self.payments_max, self.payments_min)


@dataclass(frozen=True, slots=True)
class QuietMonthPolicy:
    probability: float = field(default=0.0, metadata={"between": (0.0, 1.0)})
    skip_probability: float = field(default=0.60, metadata={"between": (0.0, 1.0)})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class RevenuePersonaProfile:
    client: CounterpartyRevenueProfile = field(
        default_factory=CounterpartyRevenueProfile
    )
    platform: CounterpartyRevenueProfile = field(
        default_factory=CounterpartyRevenueProfile
    )
    settlement: RevenueFlowProfile = field(default_factory=RevenueFlowProfile)
    owner_draw: RevenueFlowProfile = field(default_factory=RevenueFlowProfile)
    investment: RevenueFlowProfile = field(default_factory=RevenueFlowProfile)
    quiet_month: QuietMonthPolicy = field(default_factory=QuietMonthPolicy)
