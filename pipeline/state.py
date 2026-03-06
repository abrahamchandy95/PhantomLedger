from dataclasses import dataclass
from pathlib import Path

from common.config import GenerationConfig
from common.rng import Rng
from common.types import Txn
from entities.accounts import AccountsData
from entities.people import PeopleData
from entities.pii import PiiData
from entities.merchants import MerchantData
from infra.devices import DevicesData
from infra.ipaddrs import IpData
from infra.txn_infra import TxnInfraAssigner
from transfers.fraud import FraudInjectionResult
from transfers.legit import LegitTransfers


@dataclass(slots=True)
class GenerationState:
    cfg: GenerationConfig
    rng: Rng
    out_dir: Path

    people: PeopleData | None = None
    accounts: AccountsData | None = None
    pii: PiiData | None = None
    merchants: MerchantData | None = None

    devices: DevicesData | None = None
    ipdata: IpData | None = None
    infra: TxnInfraAssigner | None = None

    legit: LegitTransfers | None = None
    fraud: FraudInjectionResult | None = None

    base_txns: list[Txn] | None = None
    all_txns: list[Txn] | None = None

    unique_has_paid_edges: int = 0
