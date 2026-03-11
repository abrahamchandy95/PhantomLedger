from dataclasses import dataclass
from pathlib import Path

from common.config import GenerationConfig
from common.random import Rng
from common.transactions import Transaction
from entities.accounts import AccountsData
from entities.credit_cards import CreditCardData
from entities.merchants import MerchantData
from entities.people import PeopleData
from entities.pii import PiiData
from infra.devices import DevicesData
from infra.ipaddrs import IpData
from infra.txn_infra import TxnInfraAssigner
import transfers.fraud as fraud_model
from transfers.legit import LegitTransfers


@dataclass(frozen=True, slots=True)
class GenerationRuntime:
    cfg: GenerationConfig
    rng: Rng
    out_dir: Path


@dataclass(frozen=True, slots=True)
class EntityStageData:
    people: PeopleData
    accounts: AccountsData
    pii: PiiData
    merchants: MerchantData
    persona_for_person: dict[str, str]
    credit_cards: CreditCardData


@dataclass(frozen=True, slots=True)
class InfraStageData:
    devices: DevicesData
    ipdata: IpData
    infra: TxnInfraAssigner


@dataclass(frozen=True, slots=True)
class TransferStageData:
    legit: LegitTransfers
    fraud: fraud_model.FraudInjectionResult
    base_txns: list[Transaction]
    all_txns: list[Transaction]


@dataclass(slots=True)
class GenerationState:
    runtime: GenerationRuntime
    entities: EntityStageData | None = None
    infra: InfraStageData | None = None
    transfers: TransferStageData | None = None
    unique_has_paid_edges: int = 0

    @property
    def cfg(self) -> GenerationConfig:
        return self.runtime.cfg

    @property
    def rng(self) -> Rng:
        return self.runtime.rng

    @property
    def out_dir(self) -> Path:
        return self.runtime.out_dir

    def require_entities(self) -> EntityStageData:
        entities = self.entities
        if entities is None:
            raise RuntimeError("entity stage has not been built yet")
        return entities

    def require_infra(self) -> InfraStageData:
        infra = self.infra
        if infra is None:
            raise RuntimeError("infra stage has not been built yet")
        return infra

    def require_transfers(self) -> TransferStageData:
        transfers = self.transfers
        if transfers is None:
            raise RuntimeError("transfer stage has not been built yet")
        return transfers
