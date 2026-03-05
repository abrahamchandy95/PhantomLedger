from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path

from common.validate import (
    require_float_between,
    require_float_ge,
    require_float_gt,
    require_int_ge,
    require_int_gt,
)


def _parse_ymd(name: str, s: str) -> datetime:
    try:
        return datetime.strptime(s, "%Y-%m-%d")
    except ValueError as e:
        raise ValueError(f"{name} must be YYYY-MM-DD, got {s!r}") from e


@dataclass(frozen=True, slots=True)
class OutputConfig:
    out_dir: Path = Path("out_bank_data")
    emit_raw_ledger: bool = False


@dataclass(frozen=True, slots=True)
class WindowConfig:
    start: str = "2025-01-01"
    days: int = 180

    def start_date(self) -> datetime:
        return _parse_ymd("start", self.start)

    def validate(self) -> None:
        require_int_gt("days", self.days, 0)
        _ = self.start_date()


@dataclass(frozen=True, slots=True)
class PopulationConfig:
    seed: int = 7
    persons: int = 500_000

    def validate(self) -> None:
        require_int_gt("persons", self.persons, 0)


@dataclass(frozen=True, slots=True)
class AccountsConfig:
    max_accounts_per_person: int = 3

    def validate(self) -> None:
        require_int_gt("max_accounts_per_person", self.max_accounts_per_person, 0)


@dataclass(frozen=True, slots=True)
class HubsConfig:
    hub_fraction: float = 0.01

    def validate(self) -> None:
        require_float_between("hub_fraction", self.hub_fraction, 0.0, 0.5)


@dataclass(frozen=True, slots=True)
class FraudConfig:
    fraud_rings: int = 46
    ring_size: int = 12
    mules_per_ring: int = 4
    victims_per_ring: int = 60

    target_illicit_ratio: float = 0.00009

    def fraudsters_per_ring(self) -> int:
        ring_size = int(self.ring_size)
        mules = int(self.mules_per_ring)
        return max(1, ring_size - mules)

    def expected_fraudsters(self) -> int:
        return int(self.fraud_rings) * self.fraudsters_per_ring()

    def expected_mules(self) -> int:
        rings = int(self.fraud_rings)
        ring_size = int(self.ring_size)
        mules = int(self.mules_per_ring)
        return rings * min(mules, max(0, ring_size - 1))

    def validate(self, *, persons: int) -> None:
        require_int_ge("fraud_rings", self.fraud_rings, 0)
        require_float_between(
            "target_illicit_ratio", self.target_illicit_ratio, 0.0, 0.5
        )

        if self.fraud_rings == 0:
            return

        require_int_ge("ring_size", self.ring_size, 3)
        require_int_ge("mules_per_ring", self.mules_per_ring, 0)
        require_int_ge("victims_per_ring", self.victims_per_ring, 0)

        ring_people = int(self.fraud_rings) * int(self.ring_size)
        if ring_people >= int(persons):
            raise ValueError(
                "fraud ring participants exceed/consume the population size"
            )

        if int(self.victims_per_ring) >= int(persons) - int(self.ring_size):
            raise ValueError("victims_per_ring too large for the population size")


@dataclass(frozen=True, slots=True)
class RecurringConfig:
    salary_fraction: float = 0.65
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
        require_float_between("salary_fraction", self.salary_fraction, 0.0, 1.0)
        require_float_between("rent_fraction", self.rent_fraction, 0.0, 1.0)

        require_float_gt(
            "employer_tenure_years_min", self.employer_tenure_years_min, 0.0
        )
        if float(self.employer_tenure_years_max) < float(
            self.employer_tenure_years_min
        ):
            raise ValueError(
                "employer_tenure_years_max must be >= employer_tenure_years_min"
            )

        require_float_gt(
            "landlord_tenure_years_min", self.landlord_tenure_years_min, 0.0
        )
        if float(self.landlord_tenure_years_max) < float(
            self.landlord_tenure_years_min
        ):
            raise ValueError(
                "landlord_tenure_years_max must be >= landlord_tenure_years_min"
            )

        require_float_ge("annual_inflation_rate", self.annual_inflation_rate, 0.0)

        require_float_ge("salary_real_raise_sigma", self.salary_real_raise_sigma, 0.0)
        require_float_ge("job_switch_bump_sigma", self.job_switch_bump_sigma, 0.0)
        require_float_ge("rent_real_raise_sigma", self.rent_real_raise_sigma, 0.0)


@dataclass(frozen=True, slots=True)
class PersonasConfig:
    persona_student_frac: float = 0.12
    persona_retired_frac: float = 0.10
    persona_freelancer_frac: float = 0.10
    persona_smallbiz_frac: float = 0.06
    persona_hnw_frac: float = 0.02

    def validate(self) -> None:
        items = {
            "persona_student_frac": self.persona_student_frac,
            "persona_retired_frac": self.persona_retired_frac,
            "persona_freelancer_frac": self.persona_freelancer_frac,
            "persona_smallbiz_frac": self.persona_smallbiz_frac,
            "persona_hnw_frac": self.persona_hnw_frac,
        }

        for name, v in items.items():
            require_float_between(name, v, 0.0, 1.0)

        if float(sum(map(float, items.values()))) > 1.0:
            raise ValueError("sum of persona_*_frac must be <= 1.0")


@dataclass(frozen=True, slots=True)
class GraphConfig:
    graph_k_neighbors: int = 12
    graph_intra_household_p: float = 0.70
    graph_hub_weight_boost: float = 25.0
    graph_attractiveness_sigma: float = 1.1
    graph_edge_weight_gamma_shape: float = 1.0

    def validate(self) -> None:
        require_int_gt("graph_k_neighbors", self.graph_k_neighbors, 0)
        require_float_between(
            "graph_intra_household_p", self.graph_intra_household_p, 0.0, 1.0
        )
        require_float_gt("graph_hub_weight_boost", self.graph_hub_weight_boost, 0.0)
        require_float_gt(
            "graph_attractiveness_sigma", self.graph_attractiveness_sigma, 0.0
        )
        require_float_gt(
            "graph_edge_weight_gamma_shape", self.graph_edge_weight_gamma_shape, 0.0
        )


@dataclass(frozen=True, slots=True)
class EventsConfig:
    clearing_accounts_n: int = 3
    unknown_outflow_p: float = 0.45

    day_multiplier_gamma_shape: float = 1.3
    max_events_per_day: int = 0
    prefer_billers_p: float = 0.55

    def validate(self) -> None:
        require_int_ge("clearing_accounts_n", self.clearing_accounts_n, 0)
        require_float_between("unknown_outflow_p", self.unknown_outflow_p, 0.0, 1.0)
        require_float_gt(
            "day_multiplier_gamma_shape", self.day_multiplier_gamma_shape, 0.0
        )
        require_int_ge("max_events_per_day", self.max_events_per_day, 0)
        require_float_between("prefer_billers_p", self.prefer_billers_p, 0.0, 1.0)


@dataclass(frozen=True, slots=True)
class InfraConfig:
    infra_switch_p: float = 0.05

    def validate(self) -> None:
        require_float_between("infra_switch_p", self.infra_switch_p, 0.0, 1.0)


@dataclass(frozen=True, slots=True)
class BalancesConfig:
    enable_balance_constraints: bool = True

    overdraft_frac: float = 0.05
    overdraft_limit_median: float = 300.0
    overdraft_limit_sigma: float = 0.6

    init_bal_student: float = 200.0
    init_bal_salaried: float = 1200.0
    init_bal_retired: float = 1500.0
    init_bal_freelancer: float = 900.0
    init_bal_smallbiz: float = 8000.0
    init_bal_hnw: float = 25000.0
    init_bal_sigma: float = 1.0

    def validate(self) -> None:
        require_float_between("overdraft_frac", self.overdraft_frac, 0.0, 1.0)
        require_float_ge("overdraft_limit_median", self.overdraft_limit_median, 0.0)
        require_float_ge("overdraft_limit_sigma", self.overdraft_limit_sigma, 0.0)
        require_float_ge("init_bal_sigma", self.init_bal_sigma, 0.0)


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
        validate_all(self)


def validate_all(cfg: GenerationConfig) -> None:
    cfg.window.validate()
    cfg.population.validate()
    cfg.accounts.validate()
    cfg.hubs.validate()
    cfg.fraud.validate(persons=cfg.population.persons)
    cfg.recurring.validate()
    cfg.personas.validate()
    cfg.graph.validate()
    cfg.events.validate()
    cfg.infra.validate()
    cfg.balances.validate()


def default_config() -> GenerationConfig:
    cfg = GenerationConfig()
    cfg.validate()
    return cfg
