from dataclasses import dataclass, field

from common.transactions import Transaction

from entities import models as entity_models
from entities.counterparties import Pools as CounterpartyPools
from entities.products.portfolio import PortfolioRegistry
from infra import models as infra_models
from infra.routing import Router
from infra.shared import SharedInfra

import transfers.fraud as fraud_model
from transfers.legit import TransfersPayload


@dataclass(frozen=True, slots=True)
class Entities:
    people: entity_models.People
    accounts: entity_models.Accounts
    pii: entity_models.Pii
    merchants: entity_models.Merchants
    persona_map: dict[str, str]
    persona_objects: dict[str, entity_models.Persona]
    credit_cards: entity_models.CreditCards
    counterparty_pools: CounterpartyPools
    portfolios: PortfolioRegistry


@dataclass(frozen=True, slots=True)
class Infra:
    devices: infra_models.Devices
    ips: infra_models.Ips
    router: Router
    ring_infra: SharedInfra


@dataclass(frozen=True, slots=True)
class Transfers:
    """
    End-to-end transfer artifacts.
    """

    legit: TransfersPayload
    fraud: fraud_model.InjectionOutput
    draft_txns: list[Transaction]
    final_txns: list[Transaction]
    drop_counts: dict[str, int] = field(default_factory=dict)
