from collections.abc import Sequence
from dataclasses import dataclass, field
from datetime import datetime

from common.config import Events as events_cfg, Merchants as merchants_cfg
from common.math import F32, F64, I16, I32
from common.random import Rng
from common.transactions import Transaction
from common.validate import validate_metadata
from entities.models import Persona, Merchants
from math_models.seasonal import SeasonalConfig
from math_models.timing import Profiles
from relationships.social import Graph
from transfers.balances import ClearingHouse
from transfers.factory import TransactionFactory

from .dynamics import DynamicsConfig, PersonDynamics


@dataclass(frozen=True, slots=True)
class MerchantPickRules:
    biller_categories: tuple[str, ...] = (
        "utilities",
        "telecom",
        "insurance",
        "education",
    )
    max_pick_attempts: int = field(default=250, metadata={"ge": 1})
    max_retries: int = field(default=6, metadata={"ge": 0})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class BurstBehavior:
    """Defines the shape and likelihood of localized spending spikes."""

    probability: float = field(default=0.08, metadata={"between": (0.0, 1.0)})
    min_days: int = field(default=3, metadata={"ge": 1})
    max_days: int = field(default=9)
    multiplier: float = field(default=3.25, metadata={"gt": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)
        if self.max_days < self.min_days:
            raise ValueError(
                f"max_days ({self.max_days}) must be >= min_days ({self.min_days})"
            )


@dataclass(frozen=True, slots=True)
class ExplorationHabits:
    """How often spenders deviate from their established favorites."""

    alpha: float = field(default=1.6, metadata={"gt": 0.0})
    beta: float = field(default=9.5, metadata={"gt": 0.0})
    weekend_multiplier: float = field(default=1.25, metadata={"gt": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class LiquidityConstraints:
    """Hard and soft limits based on available wallet balance and payday cycles."""

    enabled: bool = True
    relief_days: int = field(default=2, metadata={"ge": 0})
    stress_start_day: int = field(default=3, metadata={"ge": 0})
    stress_ramp_days: int = field(default=5, metadata={"ge": 1})
    absolute_floor: float = field(default=0.08, metadata={"between": (0.0, 1.0)})
    exploration_floor: float = field(default=0.0, metadata={"between": (0.0, 1.0)})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class Parameters:
    picking_rules: MerchantPickRules = field(default_factory=MerchantPickRules)
    burst_behavior: BurstBehavior = field(default_factory=BurstBehavior)
    exploration_habits: ExplorationHabits = field(default_factory=ExplorationHabits)
    liquidity_constraints: LiquidityConstraints = field(
        default_factory=LiquidityConstraints
    )

    dynamics: DynamicsConfig = field(default_factory=DynamicsConfig)
    seasonal: SeasonalConfig = field(default_factory=SeasonalConfig)


DEFAULT_PARAMETERS = Parameters()


@dataclass(frozen=True, slots=True)
class PopulationView:
    persons: list[str]
    people_index: dict[str, int]
    primary_accounts: dict[str, str]
    personas: dict[str, str]
    persona_objects: dict[str, Persona]
    timing: Profiles


@dataclass(frozen=True, slots=True)
class MerchantView:
    merchants: Merchants
    merch_cdf: F64
    biller_cdf: F64
    fav_merchants_idx: I32
    fav_k: I16
    billers_idx: I32
    bill_k: I16
    explore_propensity: F32
    burst_start_day: I32
    burst_len: I16


@dataclass(frozen=True, slots=True)
class Market:
    start_date: datetime
    days: int

    events: events_cfg
    merchants_cfg: merchants_cfg

    population: PopulationView
    merchants: MerchantView
    social: Graph
    credit_cards: dict[str, str] = field(default_factory=dict)

    person_states: list[PersonDynamics] = field(default_factory=list)
    paydays: tuple[frozenset[int], ...] = field(default_factory=tuple)


@dataclass(frozen=True, slots=True)
class FinancialObligations:
    base_txns: Sequence[Transaction] = field(default_factory=tuple)
    base_txns_sorted: bool = False
    fixed_monthly_burden: dict[str, float] = field(default_factory=dict)


@dataclass(frozen=True, slots=True)
class TransactionEngine:
    """The machinery and infrastructure used to drive the simulation loop."""

    rng: Rng
    txf: TransactionFactory
    clearing_house: ClearingHouse | None = None


@dataclass(frozen=True, slots=True)
class PopulationCensus:
    """The raw demographic survey data before it becomes a living market."""

    persons: list[str]
    primary_accounts: dict[str, str]
    personas: dict[str, str]
    persona_objects: dict[str, Persona]
    paydays_by_person: dict[str, frozenset[int]] = field(default_factory=dict)


@dataclass(frozen=True, slots=True)
class CommercialNetwork:
    """The raw infrastructure: physical businesses and social connections."""

    merchants: Merchants
    social: Graph


@dataclass(frozen=True, slots=True)
class SimulationBounds:
    start_date: datetime
    days: int
    events: events_cfg
    merchants_cfg: merchants_cfg
