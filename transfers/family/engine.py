from dataclasses import dataclass
from datetime import datetime

import entities.models as entity_models
from common.config import Family, Window
from common.random import Rng
from relationships.family import FamilyGraph
from transfers.factory import TransactionFactory


@dataclass(frozen=True, slots=True)
class GenerateRequest:
    """The required inputs to generate family-based transactions."""

    window: Window
    params: Family
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
