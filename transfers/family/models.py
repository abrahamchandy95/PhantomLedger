from dataclasses import dataclass
from datetime import datetime

from common.config import FamilyConfig, WindowConfig
from common.random import Rng
from entities.merchants import MerchantData
from relationships.family import FamilyData
from transfers.txns import TxnFactory


@dataclass(frozen=True, slots=True)
class FamilyTransferRequest:
    window: WindowConfig
    family_cfg: FamilyConfig
    rng: Rng
    base_seed: int
    family: FamilyData
    persona_for_person: dict[str, str]
    primary_acct_for_person: dict[str, str]
    merchants: MerchantData
    txf: TxnFactory


@dataclass(frozen=True, slots=True)
class FamilyTransferSchedule:
    start_date: datetime
    end_excl: datetime
    month_starts: list[datetime]
