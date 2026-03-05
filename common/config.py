from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path


@dataclass(frozen=True, slots=True)
class OutputConfig:
    out_dir: Path = Path("out_bank_data")
    emit_raw_ledger: bool = False


@dataclass(frozen=True, slots=True)
class WindowConfig:
    start: str = "2025-01-01"
    days: int = 180

    def start_date(self) -> datetime:
        return datetime.strptime(self.start, "%Y-%m-%d")

    def validate(self) -> None:
        if self.days <= 0:
            raise ValueError("days must be > 0")
        # basic format check
        _ = self.start_date()


@dataclass(frozen=True, slots=True)
class PopulationConfig:
    seed: int = 7
    persons: int = 500_000

    def validate(self) -> None:
        if self.persons <= 0:
            raise ValueError("persons must be > 0")


@dataclass(frozen=True, slots=True)
class AccountsConfig:
    max_accounts_per_person: int = 3

    def validate(self) -> None:
        if self.max_accounts_per_person <= 0:
            raise ValueError("max_accounts_per_person must be > 0")


@dataclass(frozen=True, slots=True)
class HubsConfig:
    hub_fraction: float = 0.01

    def validate(self) -> None:
        if not (0.0 <= self.hub_fraction <= 0.5):
            raise ValueError("hub_fraction must be between 0.0 and 0.5")


@dataclass(frozen=True, slots=True)
class FraudConfig:
    fraud_rings: int = 46
    ring_size: int = 12
    mules_per_ring: int = 4
    victims_per_ring: int = 60

    target_illicit_ratio: float = 0.00009

    def fraudsters_per_ring(self) -> int:
        # ensure at least 1 fraudster per ring
        return max(1, self.ring_size - self.mules_per_ring)

    def expected_fraudsters(self) -> int:
        return self.fraud_rings * self.fraudsters_per_ring()

    def expected_mules(self) -> int:
        # mules won't exceed ring_size - 1 (at least 1 fraudster)
        return self.fraud_rings * min(self.mules_per_ring, max(0, self.ring_size - 1))

    def validate(self, *, persons: int) -> None:
        if self.fraud_rings < 0:
            raise ValueError("fraud_rings must be >= 0")

        if not (0.0 <= self.target_illicit_ratio <= 0.5):
            raise ValueError("target_illicit_ratio must be between 0.0 and 0.5")

        if self.fraud_rings == 0:
            return

        if self.ring_size < 3:
            raise ValueError("ring_size must be >= 3 when fraud_rings > 0")
        if self.mules_per_ring < 0:
            raise ValueError("mules_per_ring must be >= 0")
        if self.victims_per_ring < 0:
            raise ValueError("victims_per_ring must be >= 0")

        ring_people = self.fraud_rings * self.ring_size
        if ring_people >= persons:
            raise ValueError(
                "fraud ring participants exceed/consume the population size"
            )

        if self.victims_per_ring >= persons - self.ring_size:
            raise ValueError("victims_per_ring too large for the population size")


@dataclass(frozen=True, slots=True)
class RecurringConfig:
    # share of persons who receive salary monthly
    salary_fraction: float = 0.65
    # share of (non-hub) primary accts who pay rent monthly
    rent_fraction: float = 0.55

    employer_tenure_years_min: float = 2.0
    employer_tenure_years_max: float = 10.0
    landlord_tenure_years_min: float = 2.0
    landlord_tenure_years_max: float = 10.0

    annual_inflation_rate: float = 0.03

    salary_real_raise_mu: float = 0.02
    salary_real_raise_sigma: float = 0.01
    salary_real_raise_floor: float = 0.005

    job_switch_bump_mu: float = 0.08
    job_switch_bump_sigma: float = 0.05
    job_switch_bump_floor: float = 0.00

    rent_real_raise_mu: float = 0.03
    rent_real_raise_sigma: float = 0.02
    rent_real_raise_floor: float = 0.00

    def validate(self) -> None:
        sf = float(self.salary_fraction)
        rf = float(self.rent_fraction)

        if not (0.0 <= sf <= 1.0):
            raise ValueError("salary_fraction must be between 0.0 and 1.0")
        if not (0.0 <= rf <= 1.0):
            raise ValueError("rent_fraction must be between 0.0 and 1.0")

        if self.employer_tenure_years_min <= 0:
            raise ValueError("employer_tenure_years_min must be > 0")
        if self.employer_tenure_years_max < self.employer_tenure_years_min:
            raise ValueError(
                "employer_tenure_years_max must be >= employer_tenure_years_min"
            )

        if self.landlord_tenure_years_min <= 0:
            raise ValueError("landlord_tenure_years_min must be > 0")
        if self.landlord_tenure_years_max < self.landlord_tenure_years_min:
            raise ValueError(
                "landlord_tenure_years_max must be >= landlord_tenure_years_min"
            )

        if self.annual_inflation_rate < 0.0:
            raise ValueError("annual_inflation_rate must be >= 0")

        if self.salary_real_raise_sigma < 0.0:
            raise ValueError("salary_real_raise_sigma must be >= 0")
        if self.job_switch_bump_sigma < 0.0:
            raise ValueError("job_switch_bump_sigma must be >= 0")
        if self.rent_real_raise_sigma < 0.0:
            raise ValueError("rent_real_raise_sigma must be >= 0")


@dataclass(frozen=True, slots=True)
class PersonasConfig:
    persona_student_frac: float = 0.12
    persona_retired_frac: float = 0.10
    persona_freelancer_frac: float = 0.10
    persona_smallbiz_frac: float = 0.06
    persona_hnw_frac: float = 0.02
    # remainder -> salaried

    def validate(self) -> None:
        fracs = [
            self.persona_student_frac,
            self.persona_retired_frac,
            self.persona_freelancer_frac,
            self.persona_smallbiz_frac,
            self.persona_hnw_frac,
        ]
        for i, v in enumerate(fracs):
            if not (0.0 <= float(v) <= 1.0):
                raise ValueError(f"persona frac #{i} must be between 0.0 and 1.0")
        if float(sum(fracs)) > 1.0:
            raise ValueError("sum of persona_*_frac must be <= 1.0")


@dataclass(frozen=True, slots=True)
class GraphConfig:
    graph_k_neighbors: int = 12
    graph_intra_household_p: float = 0.70
    graph_hub_weight_boost: float = 25.0
    graph_attractiveness_sigma: float = 1.1
    graph_edge_weight_gamma_shape: float = 1.0

    def validate(self) -> None:
        if self.graph_k_neighbors <= 0:
            raise ValueError("graph_k_neighbors must be > 0")
        if not (0.0 <= self.graph_intra_household_p <= 1.0):
            raise ValueError("graph_intra_household_p must be between 0.0 and 1.0")
        if self.graph_hub_weight_boost <= 0.0:
            raise ValueError("graph_hub_weight_boost must be > 0")
        if self.graph_attractiveness_sigma <= 0.0:
            raise ValueError("graph_attractiveness_sigma must be > 0")
        if self.graph_edge_weight_gamma_shape <= 0.0:
            raise ValueError("graph_edge_weight_gamma_shape must be > 0")


@dataclass(frozen=True, slots=True)
class EventsConfig:
    # unknown outflow sinks
    clearing_accounts_n: int = 3
    unknown_outflow_p: float = 0.45

    # superposition model
    day_multiplier_gamma_shape: float = 1.3
    max_events_per_day: int = 0  # 0 = no cap
    prefer_billers_p: float = 0.55

    def validate(self) -> None:
        if self.clearing_accounts_n < 0:
            raise ValueError("clearing_accounts_n must be >= 0")
        if not (0.0 <= self.unknown_outflow_p <= 1.0):
            raise ValueError("unknown_outflow_p must be between 0.0 and 1.0")
        if self.day_multiplier_gamma_shape <= 0.0:
            raise ValueError("day_multiplier_gamma_shape must be > 0")
        if self.max_events_per_day < 0:
            raise ValueError("max_events_per_day must be >= 0")
        if not (0.0 <= self.prefer_billers_p <= 1.0):
            raise ValueError("prefer_billers_p must be between 0.0 and 1.0")


@dataclass(frozen=True, slots=True)
class InfraConfig:
    infra_switch_p: float = 0.05

    def validate(self) -> None:
        if not (0.0 <= self.infra_switch_p <= 1.0):
            raise ValueError("infra_switch_p must be between 0.0 and 1.0")


@dataclass(frozen=True, slots=True)
class BalancesConfig:
    enable_balance_constraints: bool = True

    overdraft_frac: float = 0.05
    overdraft_limit_median: float = 300.0
    overdraft_limit_sigma: float = 0.6

    # initial balances (median per persona)
    init_bal_student: float = 200.0
    init_bal_salaried: float = 1200.0
    init_bal_retired: float = 1500.0
    init_bal_freelancer: float = 900.0
    init_bal_smallbiz: float = 8000.0
    init_bal_hnw: float = 25000.0
    init_bal_sigma: float = 1.0

    def validate(self) -> None:
        if not (0.0 <= self.overdraft_frac <= 1.0):
            raise ValueError("overdraft_frac must be between 0.0 and 1.0")
        if self.overdraft_limit_median < 0.0:
            raise ValueError("overdraft_limit_median must be >= 0")
        if self.overdraft_limit_sigma < 0.0:
            raise ValueError("overdraft_limit_sigma must be >= 0")
        if self.init_bal_sigma < 0.0:
            raise ValueError("init_bal_sigma must be >= 0")


# -----------------------------
# Root config (composition)
# -----------------------------


@dataclass(frozen=True, slots=True)
class GenerationConfig:
    output: OutputConfig = field(default_factory=OutputConfig)
    window: WindowConfig = field(default_factory=WindowConfig)
    population: PopulationConfig = field(default_factory=PopulationConfig)
    accounts: AccountsConfig = field(default_factory=AccountsConfig)
    hubs: HubsConfig = field(default_factory=HubsConfig)
    fraud: FraudConfig = field(default_factory=FraudConfig)
    recurring: RecurringConfig = field(default_factory=RecurringConfig)
    personas: PersonasConfig = field(default_factory=PersonasConfig)
    graph: GraphConfig = field(default_factory=GraphConfig)
    events: EventsConfig = field(default_factory=EventsConfig)
    infra: InfraConfig = field(default_factory=InfraConfig)
    balances: BalancesConfig = field(default_factory=BalancesConfig)

    def validate(self) -> None:
        self.window.validate()
        self.population.validate()
        self.accounts.validate()
        self.hubs.validate()
        self.fraud.validate(persons=self.population.persons)
        self.recurring.validate()
        self.personas.validate()
        self.graph.validate()
        self.events.validate()
        self.infra.validate()
        self.balances.validate()


def default_config() -> GenerationConfig:
    cfg = GenerationConfig()
    cfg.validate()
    return cfg
