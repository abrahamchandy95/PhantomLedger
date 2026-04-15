from dataclasses import dataclass, field

from common.math import F64
from common.validate import validate_metadata


@dataclass(frozen=True, slots=True)
class Accounts:
    ids: list[str]
    by_person: dict[str, list[str]]
    owner_map: dict[str, str]

    frauds: set[str]
    mules: set[str]
    victims: set[str]

    externals: set[str]


@dataclass(frozen=True, slots=True)
class People:
    ids: list[str]
    frauds: set[str]
    mules: set[str]
    victims: set[str]
    solo_frauds: set[str]
    rings: list[Ring]


@dataclass(frozen=True, slots=True)
class Ring:
    id: int
    members: list[str]
    frauds: list[str]
    mules: list[str]
    victims: list[str]


@dataclass(frozen=True, slots=True)
class Persona:
    name: str
    timing_profile: str

    rate_multiplier: float = field(metadata={"gt": 0.0})
    amount_multiplier: float = field(metadata={"gt": 0.0})
    initial_balance: float = field(metadata={"gt": 0.0})
    card_prob: float = field(metadata={"between": (0.0, 1.0)})
    cc_share: float = field(metadata={"between": (0.0, 1.0)})
    credit_limit: float = field(metadata={"gt": 0.0})
    weight: float = field(metadata={"ge": 0.0})

    paycheck_sensitivity: float = field(default=0.0, metadata={"between": (0.0, 1.0)})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class Merchants:
    ids: list[str]
    counterparties: list[str]
    categories: list[str]
    weights: F64
    internals: list[str]
    externals: list[str]


@dataclass(frozen=True, slots=True)
class Pii:
    phone_map: dict[str, str]
    email_map: dict[str, str]


@dataclass(frozen=True, slots=True)
class CreditCards:
    ids: list[str]
    by_person: dict[str, str]
    owner_map: dict[str, str]
    aprs: dict[str, float]
    limits: dict[str, float]
    cycle_days: dict[str, int]
    autopay_modes: dict[str, int]
