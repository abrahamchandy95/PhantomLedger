from dataclasses import dataclass, field
from datetime import datetime

import entities.models as entity_models
from common.config import Events, Merchants
from common.math import F32, F64, I16, I32
from common.random import Rng
from common.validate import between, ge, gt
from entities.credit_cards import IssuancePolicy
from infra.routing import Router
from math_models.seasonal import SeasonalConfig
from math_models.timing import Profiles
from relationships.social import Graph

from .dynamics import DynamicsConfig, PersonDynamics


@dataclass(frozen=True, slots=True)
class Parameters:
    """Defines the statistical constraints and parameters for daily spending behavior."""

    biller_categories: tuple[str, ...] = (
        "utilities",
        "telecom",
        "insurance",
        "education",
    )

    pick_max_tries: int = 250
    merchant_retry_limit: int = 6

    burst_p: float = 0.08
    burst_min: int = 3
    burst_max: int = 9

    explore_alpha: float = 1.6
    explore_beta: float = 9.5

    weekend_boost: float = 1.25
    burst_boost: float = 3.25

    # Stronger affordability gate tied to pay-cycle distance.
    enable_liquidity_gating: bool = True
    liquidity_relief_days: int = 2
    liquidity_stress_start_day: int = 4
    liquidity_stress_ramp_days: int = 7
    liquidity_floor: float = 0.18
    liquidity_explore_floor: float = 0.05

    dynamics: DynamicsConfig = field(default_factory=DynamicsConfig)
    seasonal: SeasonalConfig = field(default_factory=SeasonalConfig)

    def __post_init__(self) -> None:
        ge("pick_max_tries", self.pick_max_tries, 1)
        ge("merchant_retry_limit", self.merchant_retry_limit, 0)

        between("burst_p", self.burst_p, 0.0, 1.0)
        ge("burst_min", self.burst_min, 1)
        ge("burst_max", self.burst_max, self.burst_min)

        gt("explore_alpha", self.explore_alpha, 0.0)
        gt("explore_beta", self.explore_beta, 0.0)

        gt("weekend_boost", self.weekend_boost, 0.0)
        gt("burst_boost", self.burst_boost, 0.0)

        ge("liquidity_relief_days", self.liquidity_relief_days, 0)
        ge("liquidity_stress_start_day", self.liquidity_stress_start_day, 0)
        ge("liquidity_stress_ramp_days", self.liquidity_stress_ramp_days, 1)
        between("liquidity_floor", self.liquidity_floor, 0.05, 1.0)
        between("liquidity_explore_floor", self.liquidity_explore_floor, 0.0, 1.0)


DEFAULT_PARAMETERS = Parameters()


@dataclass(frozen=True, slots=True)
class PopulationView:
    """Who the people are, how to reach their accounts, and their behavior."""

    persons: list[str]
    people_index: dict[str, int]
    primary_accounts: dict[str, str]
    personas: dict[str, str]
    persona_objects: dict[str, entity_models.Persona]
    timing: Profiles


@dataclass(frozen=True, slots=True)
class MerchantView:
    """The merchant pool and per-person payee assignments."""

    merchants: entity_models.Merchants
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
class Context:
    """Pre-computed state for daily generation."""

    population: PopulationView
    merchant: MerchantView
    social: Graph
    person_dynamics: list[PersonDynamics] = field(default_factory=list)
    paydays_by_person: tuple[frozenset[int], ...] = field(default_factory=tuple)


@dataclass(frozen=True, slots=True)
class BuildRequest:
    """Parameters required to build the generation context."""

    events: Events
    merchants_cfg: Merchants
    rng: Rng
    start_date: datetime
    days: int
    persons: list[str]
    primary_accounts: dict[str, str]
    personas: dict[str, str]
    persona_objects: dict[str, entity_models.Persona]
    merchants: entity_models.Merchants
    social: Graph
    base_seed: int
    paydays_by_person: dict[str, frozenset[int]] = field(default_factory=dict)
    params: Parameters = field(default_factory=Parameters)


@dataclass(frozen=True, slots=True)
class GenerateRequest:
    """Parameters required to trigger a day-to-day transaction run."""

    events: Events
    merchants_cfg: Merchants
    rng: Rng
    start_date: datetime
    days: int
    ctx: Context
    infra: Router | None = None
    credit_policy: IssuancePolicy | None = None
    cards: dict[str, str] | None = None
    params: Parameters = field(default_factory=Parameters)
