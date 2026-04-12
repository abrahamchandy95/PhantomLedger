from collections.abc import Sequence
from dataclasses import dataclass, field
from datetime import datetime

import entities.models as entity_models
import transfers.balances as balances_model
from common.config import Events, Merchants
from common.math import F32, F64, I16, I32
from common.random import Rng
from common.transactions import Transaction
from common.validate import between, ge, gt
from entities.credit_cards import IssuancePolicy
from infra.routing import Router
from math_models.seasonal import SeasonalConfig
from math_models.timing import Profiles
from relationships.social import Graph

from .dynamics import DynamicsConfig, PersonDynamics


@dataclass(frozen=True, slots=True)
class Parameters:
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

    enable_liquidity_gating: bool = True
    liquidity_relief_days: int = 2
    liquidity_stress_start_day: int = 3
    liquidity_stress_ramp_days: int = 5
    liquidity_floor: float = 0.08
    liquidity_explore_floor: float = 0.0

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
        between("liquidity_floor", self.liquidity_floor, 0.0, 1.0)
        between("liquidity_explore_floor", self.liquidity_explore_floor, 0.0, 1.0)


DEFAULT_PARAMETERS = Parameters()


@dataclass(frozen=True, slots=True)
class PopulationView:
    persons: list[str]
    people_index: dict[str, int]
    primary_accounts: dict[str, str]
    personas: dict[str, str]
    persona_objects: dict[str, entity_models.Persona]
    timing: Profiles


@dataclass(frozen=True, slots=True)
class MerchantView:
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
    population: PopulationView
    merchant: MerchantView
    social: Graph
    person_dynamics: list[PersonDynamics] = field(default_factory=list)
    paydays_by_person: tuple[frozenset[int], ...] = field(default_factory=tuple)


@dataclass(frozen=True, slots=True)
class BuildRequest:
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
    events: Events
    merchants_cfg: Merchants
    rng: Rng
    start_date: datetime
    days: int
    ctx: Context
    infra: Router | None = None
    credit_policy: IssuancePolicy | None = None
    cards: dict[str, str] | None = None
    base_txns: Sequence[Transaction] = field(default_factory=tuple)
    base_txns_sorted: bool = False
    fixed_monthly_burden: dict[str, float] = field(default_factory=dict)
    screen_book: balances_model.Ledger | None = None
    params: Parameters = field(default_factory=Parameters)
