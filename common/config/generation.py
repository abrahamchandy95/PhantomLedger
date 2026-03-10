from dataclasses import dataclass, field

from .runtime.output import OutputConfig
from .runtime.window import WindowConfig

from .population.population import PopulationConfig
from .population.accounts import AccountsConfig
from .population.hubs import HubsConfig
from .population.personas import PersonasConfig
from .population.merchants import MerchantsConfig
from .population.family import FamilyConfig

from .simulation.fraud import FraudConfig
from .simulation.events import EventsConfig
from .simulation.infra import InfraConfig


@dataclass(frozen=True, slots=True)
class GenerationConfig:
    output: OutputConfig = field(default_factory=OutputConfig)
    window: WindowConfig = field(default_factory=WindowConfig)

    population: PopulationConfig = field(default_factory=PopulationConfig)
    accounts: AccountsConfig = field(default_factory=AccountsConfig)
    hubs: HubsConfig = field(default_factory=HubsConfig)
    personas: PersonasConfig = field(default_factory=PersonasConfig)
    merchants: MerchantsConfig = field(default_factory=MerchantsConfig)
    family: FamilyConfig = field(default_factory=FamilyConfig)

    fraud: FraudConfig = field(default_factory=FraudConfig)
    events: EventsConfig = field(default_factory=EventsConfig)
    infra: InfraConfig = field(default_factory=InfraConfig)

    def validate(self) -> None:
        validate_all(self)


def validate_all(cfg: GenerationConfig) -> None:
    cfg.window.validate()

    cfg.population.validate()
    cfg.accounts.validate()
    cfg.hubs.validate()
    cfg.personas.validate()
    cfg.merchants.validate()
    cfg.family.validate()

    cfg.fraud.validate(persons=cfg.population.persons)
    cfg.events.validate()
    cfg.infra.validate()


def default_config() -> GenerationConfig:
    cfg = GenerationConfig()
    cfg.validate()
    return cfg
