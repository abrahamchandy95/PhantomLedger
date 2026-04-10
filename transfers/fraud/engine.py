from dataclasses import dataclass, field
from datetime import datetime
from typing import Literal

import entities.models as models
from common.config import Fraud, Window
from common.random import Rng
from common.transactions import Transaction
from common.validate import between, ge, gt
from infra.routing import Router
from infra.shared import SharedInfra
from transfers.factory import TransactionFactory

# --- Parameter Definitions ---

Typology = Literal["classic", "layering", "funnel", "structuring", "invoice", "mule"]


@dataclass(frozen=True, slots=True)
class TypologyWeights:
    classic: float = 0.30
    layering: float = 0.15
    funnel: float = 0.10
    structuring: float = 0.10
    invoice: float = 0.05
    mule: float = 0.30

    def __post_init__(self) -> None:
        weights = (
            self.classic,
            self.layering,
            self.funnel,
            self.structuring,
            self.invoice,
            self.mule,
        )
        if any(w < 0.0 for w in weights):
            raise ValueError("Fraud typology weights must be >= 0")

    def choose(self, rng: Rng) -> Typology:
        items: tuple[Typology, ...] = (
            "classic",
            "layering",
            "funnel",
            "structuring",
            "invoice",
            "mule",
        )
        weights = (
            float(self.classic),
            float(self.layering),
            float(self.funnel),
            float(self.structuring),
            float(self.invoice),
            float(self.mule),
        )
        if float(sum(weights)) <= 0.0:
            return "classic"
        return rng.weighted_choice(items, weights)


@dataclass(frozen=True, slots=True)
class LayeringRules:
    min_hops: int = 3
    max_hops: int = 8

    def __post_init__(self) -> None:
        ge("min_hops", self.min_hops, 1)
        ge("max_hops", self.max_hops, self.min_hops)


@dataclass(frozen=True, slots=True)
class StructuringRules:
    threshold: float = 10_000.0
    epsilon_min: float = 50.0
    epsilon_max: float = 400.0
    splits_min: int = 3
    splits_max: int = 12

    def __post_init__(self) -> None:
        gt("threshold", self.threshold, 0.0)
        if float(self.epsilon_max) < float(self.epsilon_min):
            raise ValueError("epsilon_max must be >= epsilon_min")
        ge("splits_min", self.splits_min, 1)
        ge("splits_max", self.splits_max, self.splits_min)


@dataclass(frozen=True, slots=True)
class CamouflageRates:
    small_p2p_per_day_p: float = 0.03
    bill_monthly_p: float = 0.35
    salary_inbound_p: float = 0.12

    def __post_init__(self) -> None:
        between("small_p2p_per_day_p", self.small_p2p_per_day_p, 0.0, 1.0)
        between("bill_monthly_p", self.bill_monthly_p, 0.0, 1.0)
        between("salary_inbound_p", self.salary_inbound_p, 0.0, 1.0)


@dataclass(frozen=True, slots=True)
class Parameters:
    typology: TypologyWeights = field(default_factory=TypologyWeights)
    layering: LayeringRules = field(default_factory=LayeringRules)
    structuring: StructuringRules = field(default_factory=StructuringRules)
    camouflage: CamouflageRates = field(default_factory=CamouflageRates)


# --- Request and Input Models ---


@dataclass(frozen=True, slots=True)
class Scenario:
    fraud_cfg: Fraud
    window: Window
    people: models.People
    accounts: models.Accounts
    base_txns: list[Transaction]


@dataclass(frozen=True, slots=True)
class Runtime:
    rng: Rng
    infra: Router | None = None
    ring_infra: SharedInfra | None = None


@dataclass(frozen=True, slots=True)
class Counterparties:
    biller_accounts: tuple[str, ...] = ()
    employers: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True)
class InjectionInput:
    scenario: Scenario
    runtime: Runtime
    counterparties: Counterparties = field(default_factory=Counterparties)
    params: Parameters = field(default_factory=Parameters)


@dataclass(frozen=True, slots=True)
class InjectionOutput:
    txns: list[Transaction]
    injected_count: int


# --- Execution Contexts ---


@dataclass(frozen=True, slots=True)
class Execution:
    txf: TransactionFactory
    rng: Rng


@dataclass(frozen=True, slots=True)
class ActiveWindow:
    start_date: datetime
    days: int


@dataclass(frozen=True, slots=True)
class AccountPools:
    all_accounts: tuple[str, ...]
    biller_accounts: tuple[str, ...]
    employers: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class CamouflageContext:
    execution: Execution
    window: ActiveWindow
    accounts: AccountPools
    rates: CamouflageRates


@dataclass(frozen=True, slots=True)
class IllicitContext:
    execution: Execution
    window: ActiveWindow
    biller_accounts: tuple[str, ...]
    layering_rules: LayeringRules
    structuring_rules: StructuringRules
