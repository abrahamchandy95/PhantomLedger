from dataclasses import dataclass, field
from math import exp

from .runtime.window import Window

from .population.totals import Population
from .population.accounts import Accounts
from .population.hubs import Hubs
from .population.personas import Personas
from .population.merchants import Merchants
from .population.family import Family
from .population.government import Government
from .population.insurance import Insurance

from .simulation.events import Events
from .simulation.infra import Infra
from .simulation.fraud import Fraud, Patterns


@dataclass(frozen=True, slots=True)
class World:
    window: Window = field(default_factory=Window)

    population: Population = field(default_factory=Population)
    accounts: Accounts = field(default_factory=Accounts)
    hubs: Hubs = field(default_factory=Hubs)
    personas: Personas = field(default_factory=Personas)
    merchants: Merchants = field(default_factory=Merchants)
    family: Family = field(default_factory=Family)
    government: Government = field(default_factory=Government)
    insurance: Insurance = field(default_factory=Insurance)

    events: Events = field(default_factory=Events)
    infra: Infra = field(default_factory=Infra)
    fraud: Fraud = field(default_factory=Fraud)
    patterns: Patterns = field(default_factory=Patterns)

    def __post_init__(self) -> None:
        """
        Validates the tree. Note: Sub-configs now validate themselves
        on creation via their own __post_init__.
        """
        self._check_fraud_limits()

    def _check_fraud_limits(self) -> None:
        """Ensures expected fraud participants fit within population ceiling."""
        size = self.population.size
        f = self.fraud

        ring_count = f.expected_ring_count(size)
        avg_ring_size = exp(float(f.ring_size_mu))

        expected_members = int(ring_count * avg_ring_size)
        expected_solos = f.expected_solo_count(size)
        total = expected_members + expected_solos

        ceiling = f.max_participants(size)

        if total > ceiling:
            raise ValueError(
                f"Expected fraud participants (~{total}) exceed "
                + f"limit ({ceiling} for {size} people). "
                + "Reduce rings_per_10k_mean, ring_size_mu, or solos_per_10k."
            )

        if ceiling >= size:
            raise ValueError("max_participation_p too high for population size")
