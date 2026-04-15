from dataclasses import dataclass, field

from .runtime.window import Window

from .population.totals import Population
from .population.accounts import Accounts
from .population.hubs import Hubs
from .population.personas import Personas
from .population.merchants import Merchants
from .population.family import (
    Allowances,
    Dependents,
    GrandparentGifts,
    Households,
    Inheritance,
    ParentGifts,
    RetireeSupport,
    Routing,
    SiblingTransfers,
    Spouses,
    Tuition,
)
from .population.government import Government
from .population.insurance import Insurance

from .simulation.events import Events
from .simulation.infra import Infra
from .simulation.fraud import Fraud, Patterns


@dataclass(frozen=True, slots=True)
class MarketConditions:
    """The overarching timeframe and macro rules for buyers of goods."""

    window: Window = field(default_factory=Window)
    outflow_rules: Events = field(default_factory=Events)
    merchants: Merchants = field(default_factory=Merchants)
    base_seed: int = 0


@dataclass(frozen=True, slots=True)
class World:
    window: Window = field(default_factory=Window)

    population: Population = field(default_factory=Population)
    accounts: Accounts = field(default_factory=Accounts)
    hubs: Hubs = field(default_factory=Hubs)
    personas: Personas = field(default_factory=Personas)
    merchants: Merchants = field(default_factory=Merchants)

    households: Households = field(default_factory=Households)
    dependents: Dependents = field(default_factory=Dependents)
    allowances: Allowances = field(default_factory=Allowances)
    tuition: Tuition = field(default_factory=Tuition)
    retiree_support: RetireeSupport = field(default_factory=RetireeSupport)
    spouses: Spouses = field(default_factory=Spouses)
    parent_gifts: ParentGifts = field(default_factory=ParentGifts)
    sibling_transfers: SiblingTransfers = field(default_factory=SiblingTransfers)
    grandparent_gifts: GrandparentGifts = field(default_factory=GrandparentGifts)
    inheritance: Inheritance = field(default_factory=Inheritance)
    family_routing: Routing = field(default_factory=Routing)

    government: Government = field(default_factory=Government)
    insurance: Insurance = field(default_factory=Insurance)

    events: Events = field(default_factory=Events)
    infra: Infra = field(default_factory=Infra)
    fraud: Fraud = field(default_factory=Fraud)
    patterns: Patterns = field(default_factory=Patterns)

    def __post_init__(self) -> None:
        self._check_fraud_limits()

    def _check_fraud_limits(self) -> None:
        """Ensures expected fraud participants fit within population ceiling."""
        size = self.population.size
        f = self.fraud

        ring_count = f.expected_ring_count(size)
        avg_ring_size = f.expected_ring_size_mean()

        expected_members = int(round(ring_count * avg_ring_size))
        expected_solos = f.expected_solo_count(size)
        total = expected_members + expected_solos

        ceiling = f.max_participants(size)

        if total > ceiling:
            raise ValueError(
                f"Expected fraud participants (~{total}) exceed "
                + f"limit ({ceiling} for {size} people). "
                + "Reduce rings_per_10k_mean, ring_size_mu, ring_size_sigma, "
                + "or solos_per_10k."
            )

        if ceiling >= size:
            raise ValueError("max_participation_p too high for population size")
