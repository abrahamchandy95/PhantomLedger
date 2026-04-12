from dataclasses import dataclass, field

import entities.credit_cards as credit_cards_entity
import entities.models as models
import relationships.recurring as recurring_model
import transfers.balances as balances_model
import transfers.credit_cards as credit_cards_transfer
from common import config
from common.random import Rng
from common.transactions import Transaction
from entities.counterparties import Pools as CounterpartyPools
from entities.products.portfolio import PortfolioRegistry
from infra.routing import Router


@dataclass(frozen=True, slots=True)
class TransfersPayload:
    """
    Semantic-order legit candidates plus the pristine starting ledger.
    """

    candidate_txns: list[Transaction]
    hub_accounts: list[str]
    biller_accounts: list[str]
    employers: list[str]
    initial_book: balances_model.Ledger | None = None


@dataclass(frozen=True, slots=True)
class CreditCardProfile:
    terms: credit_cards_transfer.Terms = field(
        default_factory=credit_cards_transfer.Terms
    )
    habits: credit_cards_transfer.Habits = field(
        default_factory=credit_cards_transfer.Habits
    )


@dataclass(frozen=True, slots=True)
class Specifications:
    recurring: recurring_model.Policy = field(default_factory=recurring_model.Policy)
    social: config.Social = field(default_factory=config.Social)
    balances: balances_model.Rules = field(default_factory=balances_model.Rules)
    credit_issuance: credit_cards_entity.IssuancePolicy = field(
        default_factory=credit_cards_entity.IssuancePolicy
    )
    cc_profile: CreditCardProfile = field(default_factory=CreditCardProfile)


DEFAULT_LEGIT_POLICIES = Specifications()


@dataclass(frozen=True, slots=True)
class LegitCreditRuntime:
    cards: models.CreditCards | None = None

    def enabled(self) -> bool:
        return bool(self.cards is not None and self.cards.ids)


@dataclass(frozen=True, slots=True)
class LegitInputs:
    window: config.Window
    pop: config.Population
    hubs: config.Hubs
    personas: config.Personas
    events: config.Events
    merchants_cfg: config.Merchants
    rng: Rng
    accounts: models.Accounts
    merchants: models.Merchants
    portfolios: PortfolioRegistry | None = None
    government: config.Government = field(default_factory=config.Government)


@dataclass(frozen=True, slots=True)
class LegitOverrides:
    infra: Router | None = None
    persona_names: list[str] | None = None
    persona_for_person: dict[str, str] | None = None
    persona_objects: dict[str, models.Persona] | None = None
    family_cfg: config.Family | None = None
    counterparty_pools: CounterpartyPools | None = None


@dataclass(frozen=True, slots=True)
class LegitGenerationRequest:
    inputs: LegitInputs
    specs: Specifications = field(default_factory=Specifications)
    overrides: LegitOverrides = field(default_factory=LegitOverrides)
    credit_runtime: LegitCreditRuntime = field(default_factory=LegitCreditRuntime)
