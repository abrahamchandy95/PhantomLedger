from dataclasses import dataclass, field

from common.config import (
    EventsConfig,
    FamilyConfig,
    HubsConfig,
    MerchantsConfig,
    PersonasConfig,
    PopulationConfig,
    WindowConfig,
)
from common.random import Rng
from common.types import Txn
from entities.accounts import AccountsData
import entities.credit_cards as credit_cards_entity
from entities.merchants import MerchantData
from infra.txn_infra import TxnInfraAssigner
import relationships.recurring as recurring_model
import relationships.social as social_model
import transfers.balances as balances_model
import transfers.credit_cards as credit_cards_transfer


@dataclass(frozen=True, slots=True)
class LegitTransfers:
    txns: list[Txn]
    hub_accounts: list[str]
    biller_accounts: list[str]
    employers: list[str]


@dataclass(frozen=True, slots=True)
class LegitPolicies:
    recurring: recurring_model.RecurringPolicy = field(
        default_factory=recurring_model.RecurringPolicy
    )
    social: social_model.SocialGraphPolicy = field(
        default_factory=social_model.SocialGraphPolicy
    )
    balances: balances_model.BalancePolicy = field(
        default_factory=balances_model.BalancePolicy
    )
    credit_issuance: credit_cards_entity.CreditIssuancePolicy = field(
        default_factory=credit_cards_entity.CreditIssuancePolicy
    )
    credit_lifecycle: credit_cards_transfer.CreditLifecyclePolicy = field(
        default_factory=credit_cards_transfer.CreditLifecyclePolicy
    )

    def validate(self) -> None:
        self.recurring.validate()
        self.social.validate()
        self.balances.validate()
        self.credit_issuance.validate()
        self.credit_lifecycle.validate()


DEFAULT_LEGIT_POLICIES = LegitPolicies()


@dataclass(frozen=True, slots=True)
class LegitCreditRuntime:
    cards: credit_cards_entity.CreditCardData | None = None

    def enabled(self) -> bool:
        return bool(self.cards is not None and self.cards.card_for_person)


@dataclass(frozen=True, slots=True)
class LegitInputs:
    window: WindowConfig
    pop: PopulationConfig
    hubs: HubsConfig
    personas: PersonasConfig
    events: EventsConfig
    merchants_cfg: MerchantsConfig
    rng: Rng
    accounts: AccountsData
    merchants: MerchantData


@dataclass(frozen=True, slots=True)
class LegitOverrides:
    infra: TxnInfraAssigner | None = None
    persona_names: list[str] | None = None
    persona_for_person: dict[str, str] | None = None
    family_cfg: FamilyConfig | None = None


@dataclass(frozen=True, slots=True)
class LegitGenerationRequest:
    inputs: LegitInputs
    policies: LegitPolicies = field(default_factory=LegitPolicies)
    overrides: LegitOverrides = field(default_factory=LegitOverrides)
    credit_runtime: LegitCreditRuntime = field(default_factory=LegitCreditRuntime)
