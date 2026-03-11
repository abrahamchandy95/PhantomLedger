from dataclasses import dataclass, field

from common.config import FraudConfig, WindowConfig
from common.random import Rng
from common.transactions import Transaction
from entities.accounts import AccountsData
from entities.people import PeopleData
from infra.txn_infra import TxnInfraAssigner

from .policy import FraudPolicies


@dataclass(frozen=True, slots=True)
class FraudScenario:
    fraud_cfg: FraudConfig
    window: WindowConfig
    people: PeopleData
    accounts: AccountsData
    base_txns: list[Transaction]


@dataclass(frozen=True, slots=True)
class FraudRuntime:
    rng: Rng
    infra: TxnInfraAssigner | None = None


@dataclass(frozen=True, slots=True)
class FraudCounterparties:
    biller_accounts: tuple[str, ...] = ()
    employers: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True)
class FraudInjectionRequest:
    scenario: FraudScenario
    runtime: FraudRuntime
    counterparties: FraudCounterparties = field(default_factory=FraudCounterparties)
    policies: FraudPolicies = field(default_factory=FraudPolicies)


@dataclass(frozen=True, slots=True)
class FraudInjectionResult:
    txns: list[Transaction]
    injected_count: int
