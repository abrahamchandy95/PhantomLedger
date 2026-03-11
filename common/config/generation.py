from dataclasses import dataclass, field

# Runtime
from .runtime.output import OutputConfig
from .runtime.window import WindowConfig

# Population
from .population.totals import PopulationConfig
from .population.accounts import AccountsConfig
from .population.hubs import HubsConfig
from .population.personas import PersonasConfig
from .population.merchants import MerchantsConfig
from .population.family import FamilyConfig

# Simulation
from .simulation.fraud import FraudConfig
from .simulation.events import EventsConfig
from .simulation.infra import InfraConfig


@dataclass(frozen=True, slots=True)
class GenerationConfig:
    # Runtime Configs
    output: OutputConfig = field(default_factory=OutputConfig)
    window: WindowConfig = field(default_factory=WindowConfig)

    # Population Configs
    population: PopulationConfig = field(default_factory=PopulationConfig)
    accounts: AccountsConfig = field(default_factory=AccountsConfig)
    hubs: HubsConfig = field(default_factory=HubsConfig)
    personas: PersonasConfig = field(default_factory=PersonasConfig)
    merchants: MerchantsConfig = field(default_factory=MerchantsConfig)
    family: FamilyConfig = field(default_factory=FamilyConfig)

    # Simulation Configs
    fraud: FraudConfig = field(default_factory=FraudConfig)
    events: EventsConfig = field(default_factory=EventsConfig)
    infra: InfraConfig = field(default_factory=InfraConfig)

    def validate(self) -> None:
        """Validates the entire configuration tree."""
        self.window.validate()

        self.population.validate()
        self.accounts.validate()
        self.hubs.validate()
        self.personas.validate()
        self.merchants.validate()
        self.family.validate()

        # Fraud validation requires cross-config dependency
        self.fraud.validate(persons=self.population.size)
        self.events.validate()
        self.infra.validate()

    @classmethod
    def create_default(cls) -> "GenerationConfig":
        """Instantiates and validates a default GenerationConfig."""
        config = cls()
        config.validate()
        return config
