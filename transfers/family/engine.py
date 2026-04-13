from dataclasses import dataclass
from datetime import datetime

import entities.models as entity_models
from common.config import population as pop_config
from common.config import Window
from common.random import Rng
from relationships.family import FamilyGraph
from transfers.factory import TransactionFactory


@dataclass(frozen=True, slots=True)
class Runtime:
    """The required inputs to generate family-based transactions."""

    window: Window
    rng: Rng
    base_seed: int
    family: FamilyGraph
    personas: dict[str, str]
    persona_objects: dict[str, entity_models.Persona]
    primary_accounts: dict[str, str]
    merchants: entity_models.Merchants
    txf: TransactionFactory


@dataclass(frozen=True, slots=True)
class Schedule:
    """The temporal boundaries and active months for family generation."""

    start_date: datetime
    end_excl: datetime
    month_starts: list[datetime]


@dataclass(frozen=True, slots=True)
class GraphConfig:
    households: pop_config.Households
    dependents: pop_config.Dependents
    retiree_support: pop_config.RetireeSupport


@dataclass(frozen=True, slots=True)
class TransferConfig:
    allowances: pop_config.Allowances
    tuition: pop_config.Tuition
    retiree_support: pop_config.RetireeSupport
    spouses: pop_config.Spouses
    parent_gifts: pop_config.ParentGifts
    sibling_transfers: pop_config.SiblingTransfers
    grandparent_gifts: pop_config.GrandparentGifts
    inheritance: pop_config.Inheritance
    routing: pop_config.Routing
