from dataclasses import dataclass

from common.math import F64
from common.validate import between, ge, gt


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
    rate_multiplier: float
    amount_multiplier: float
    timing_profile: str
    initial_balance: float
    card_prob: float
    cc_share: float
    credit_limit: float
    weight: float
    paycheck_sensitivity: float = 0.0

    def __post_init__(self) -> None:
        """Validates inputs the moment the dataclass is instantiated."""
        gt("rate_multiplier", self.rate_multiplier, 0.0)
        gt("amount_multiplier", self.amount_multiplier, 0.0)
        gt("initial_balance", self.initial_balance, 0.0)
        between("card_prob", self.card_prob, 0.0, 1.0)
        between("cc_share", self.cc_share, 0.0, 1.0)
        gt("credit_limit", self.credit_limit, 0.0)
        ge("weight", self.weight, 0.0)
        between("paycheck_sensitivity", self.paycheck_sensitivity, 0.0, 1.0)


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
