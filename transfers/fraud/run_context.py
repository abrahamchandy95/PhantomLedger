from dataclasses import dataclass
from datetime import datetime

from common.random import Rng
from transfers.txns import TxnFactory

from .policy import CamouflagePolicy, LayeringPolicy, StructuringPolicy


@dataclass(frozen=True, slots=True)
class FraudExecution:
    txf: TxnFactory
    rng: Rng


@dataclass(frozen=True, slots=True)
class FraudWindow:
    start_date: datetime
    days: int


@dataclass(frozen=True, slots=True)
class FraudAccountPools:
    all_accounts: tuple[str, ...]
    biller_accounts: tuple[str, ...]
    employers: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class CamouflageContext:
    execution: FraudExecution
    window: FraudWindow
    accounts: FraudAccountPools
    policy: CamouflagePolicy


@dataclass(frozen=True, slots=True)
class IllicitContext:
    execution: FraudExecution
    window: FraudWindow
    biller_accounts: tuple[str, ...]
    layering_policy: LayeringPolicy
    structuring_policy: StructuringPolicy
