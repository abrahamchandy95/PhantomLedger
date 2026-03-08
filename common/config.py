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

        eligible = int(persons) - ring_people
        if int(self.victims_per_ring) > eligible:
            raise ValueError(
                "victims_per_ring too large for eligible non-ring population"
            )


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

        for name, value in items.items():
            require_float_between(name, value, 0.0, 1.0)

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
class MerchantsConfig:
    # Merchant count scales with population (keeps things stable when you change persons)
    merchants_per_10k_people: float = 12.0  # 0.12% of population as merchants

    # What fraction of merchants bank with us (have internal M... accounts)?
    in_bank_merchant_frac: float = 0.12

    # Merchant "size" (traffic) heavy-tail via lognormal
    size_lognormal_sigma: float = 1.2

    # Stickiness: for each payer, how many regular merchants/billers
    fav_merchants_min: int = 8
    fav_merchants_max: int = 30
    billers_min: int = 2
    billers_max: int = 6

    # Exploration probability per merchant payment (low => sticky)
    explore_p: float = 0.02

    # Channel mix for bank-visible outgoing payments.
    # external_unknown is retained here for backward compatibility, but the
    # active day-to-day channel mix now uses EventsConfig.unknown_outflow_p.
    share_merchant: float = 0.68
    share_bills: float = 0.20
    share_p2p: float = 0.10
    share_external_unknown: float = 0.02

    # Bank-visible payment intensity (NOT all payments overall; choose a subset)
    target_payments_per_person_per_month: float = 18.0

    def validate(self) -> None:
        require_float_ge("merchants_per_10k_people", self.merchants_per_10k_people, 0.0)
        require_float_between(
            "in_bank_merchant_frac", self.in_bank_merchant_frac, 0.0, 1.0
        )
        require_float_gt("size_lognormal_sigma", self.size_lognormal_sigma, 0.0)

        require_int_ge("fav_merchants_min", self.fav_merchants_min, 1)
        require_int_ge(
            "fav_merchants_max", self.fav_merchants_max, self.fav_merchants_min
        )

        require_int_ge("billers_min", self.billers_min, 1)
        require_int_ge("billers_max", self.billers_max, self.billers_min)

        require_float_between("explore_p", self.explore_p, 0.0, 1.0)

        shares = (
            float(self.share_merchant),
            float(self.share_bills),
            float(self.share_p2p),
            float(self.share_external_unknown),
        )
        if any(s < 0.0 for s in shares):
            raise ValueError("merchant/bill/p2p/external shares must be >= 0")
        if float(sum(shares)) <= 0.0:
            raise ValueError("sum of shares must be > 0")

        require_float_ge(
            "target_payments_per_person_per_month",
            self.target_payments_per_person_per_month,
            0.0,
        )


@dataclass(frozen=True, slots=True)
class FamilyConfig:
    # --- household structure ---
    single_household_frac: float = 0.29
    household_zipf_alpha: float = 2.2
    household_max_size: int = 6

    spouse_pair_p: float = 0.62

    # --- dependency structure ---
    student_dependent_p: float = 0.65
    co_reside_student_p: float = 0.35  # fraction of dependent students co-residing
    two_parent_p: float = 0.70  # if dependent, probability of 2 parents vs 1

    # --- allowance process ---
    allowance_enabled: bool = True
    allowance_weekly_p: float = 0.60  # else biweekly
    allowance_pareto_xm: float = 15.0  # minimum allowance
    allowance_pareto_alpha: float = 2.2  # tail index

    # --- tuition process ---
    tuition_enabled: bool = True
    tuition_students_p: float = 0.55  # fraction of students who pay tuition in-window
    tuition_installments_min: int = 4
    tuition_installments_max: int = 5
    tuition_total_logn_mu: float = 8.7
    tuition_total_logn_sigma: float = 0.35

    # --- support to retirees ---
    retiree_support_enabled: bool = True

    # Probability a retired person is linked to at least one adult child
    # for potential support. This is separate from monthly payment activity.
    retiree_has_adult_child_p: float = 0.60

    # Conditional on being linked to adult child(ren), probability that
    # support is actually sent in a given month.
    retiree_support_p: float = 0.35

    # Minority case: support child is co-resident with the retiree.
    retiree_support_co_reside_p: float = 0.12

    retiree_support_pareto_xm: float = 25.0
    retiree_support_pareto_alpha: float = 2.4

    def validate(self) -> None:
        require_float_between(
            "single_household_frac", self.single_household_frac, 0.0, 1.0
        )
        require_float_gt("household_zipf_alpha", self.household_zipf_alpha, 0.0)
        require_int_ge("household_max_size", self.household_max_size, 2)

        require_float_between("spouse_pair_p", self.spouse_pair_p, 0.0, 1.0)

        require_float_between("student_dependent_p", self.student_dependent_p, 0.0, 1.0)
        require_float_between("co_reside_student_p", self.co_reside_student_p, 0.0, 1.0)
        require_float_between("two_parent_p", self.two_parent_p, 0.0, 1.0)

        require_float_between("allowance_weekly_p", self.allowance_weekly_p, 0.0, 1.0)
        require_float_gt("allowance_pareto_xm", self.allowance_pareto_xm, 0.0)
        require_float_gt("allowance_pareto_alpha", self.allowance_pareto_alpha, 0.0)

        require_float_between("tuition_students_p", self.tuition_students_p, 0.0, 1.0)
        require_int_ge("tuition_installments_min", self.tuition_installments_min, 1)
        require_int_ge(
            "tuition_installments_max",
            self.tuition_installments_max,
            self.tuition_installments_min,
        )

        require_float_between(
            "retiree_has_adult_child_p", self.retiree_has_adult_child_p, 0.0, 1.0
        )
        require_float_between("retiree_support_p", self.retiree_support_p, 0.0, 1.0)
        require_float_between(
            "retiree_support_co_reside_p",
            self.retiree_support_co_reside_p,
            0.0,
            1.0,
        )
        require_float_gt(
            "retiree_support_pareto_xm", self.retiree_support_pareto_xm, 0.0
        )
        require_float_gt(
            "retiree_support_pareto_alpha", self.retiree_support_pareto_alpha, 0.0
        )


@dataclass(frozen=True, slots=True)
class CreditConfig:
    enable_credit_cards: bool = True

    # --- card ownership by persona ---
    card_owner_student_p: float = 0.25
    card_owner_salaried_p: float = 0.70
    card_owner_retired_p: float = 0.55
    card_owner_freelancer_p: float = 0.65
    card_owner_smallbiz_p: float = 0.80
    card_owner_hnw_p: float = 0.92

    # --- how often a merchant purchase is paid by credit (conditional on having a card) ---
    cc_share_merchant_student: float = 0.55
    cc_share_merchant_salaried: float = 0.70
    cc_share_merchant_retired: float = 0.55
    cc_share_merchant_freelancer: float = 0.65
    cc_share_merchant_smallbiz: float = 0.75
    cc_share_merchant_hnw: float = 0.80

    # --- card economics ---
    apr_median: float = 0.22
    apr_sigma: float = 0.25
    apr_min: float = 0.08
    apr_max: float = 0.36

    limit_sigma: float = 0.65
    limit_student_median: float = 800.0
    limit_salaried_median: float = 3000.0
    limit_retired_median: float = 2500.0
    limit_freelancer_median: float = 4000.0
    limit_smallbiz_median: float = 7000.0
    limit_hnw_median: float = 15000.0

    # --- billing cycle ---
    cycle_day_min: int = 1
    cycle_day_max: int = 28
    grace_days: int = 25

    # --- min payment ---
    min_payment_pct: float = 0.02
    min_payment_dollars: float = 25.0

    # --- autopay configuration (per card) ---
    autopay_full_p: float = 0.40
    autopay_min_p: float = 0.10

    # --- manual pay behavior (if no autopay) ---
    manual_pay_full_p: float = 0.35
    manual_pay_partial_p: float = 0.30
    manual_pay_min_p: float = 0.25
    manual_miss_p: float = 0.10

    # Partial payment fraction beyond minimum: Beta(a, b)
    partial_beta_a: float = 2.0
    partial_beta_b: float = 5.0

    # Chance a payment is late even if they intend to pay
    late_payment_p: float = 0.08
    late_days_min: int = 1
    late_days_max: int = 20

    late_fee_amount: float = 32.0

    # Refunds / chargebacks as credits back to card (external -> card)
    refund_p: float = 0.006
    refund_delay_days_min: int = 1
    refund_delay_days_max: int = 14

    chargeback_p: float = 0.001
    chargeback_delay_days_min: int = 7
    chargeback_delay_days_max: int = 45

    def owner_p(self, persona: str) -> float:
        match persona:
            case "student":
                return float(self.card_owner_student_p)
            case "retired":
                return float(self.card_owner_retired_p)
            case "freelancer":
                return float(self.card_owner_freelancer_p)
            case "smallbiz":
                return float(self.card_owner_smallbiz_p)
            case "hnw":
                return float(self.card_owner_hnw_p)
            case _:
                return float(self.card_owner_salaried_p)

    def cc_share_for_merchant(self, persona: str) -> float:
        match persona:
            case "student":
                return float(self.cc_share_merchant_student)
            case "retired":
                return float(self.cc_share_merchant_retired)
            case "freelancer":
                return float(self.cc_share_merchant_freelancer)
            case "smallbiz":
                return float(self.cc_share_merchant_smallbiz)
            case "hnw":
                return float(self.cc_share_merchant_hnw)
            case _:
                return float(self.cc_share_merchant_salaried)

    def limit_median(self, persona: str) -> float:
        match persona:
            case "student":
                return float(self.limit_student_median)
            case "retired":
                return float(self.limit_retired_median)
            case "freelancer":
                return float(self.limit_freelancer_median)
            case "smallbiz":
                return float(self.limit_smallbiz_median)
            case "hnw":
                return float(self.limit_hnw_median)
            case _:
                return float(self.limit_salaried_median)

    def validate(self) -> None:
        probability_fields: tuple[tuple[str, float], ...] = (
            ("card_owner_student_p", self.card_owner_student_p),
            ("card_owner_salaried_p", self.card_owner_salaried_p),
            ("card_owner_retired_p", self.card_owner_retired_p),
            ("card_owner_freelancer_p", self.card_owner_freelancer_p),
            ("card_owner_smallbiz_p", self.card_owner_smallbiz_p),
            ("card_owner_hnw_p", self.card_owner_hnw_p),
            ("cc_share_merchant_student", self.cc_share_merchant_student),
            ("cc_share_merchant_salaried", self.cc_share_merchant_salaried),
            ("cc_share_merchant_retired", self.cc_share_merchant_retired),
            ("cc_share_merchant_freelancer", self.cc_share_merchant_freelancer),
            ("cc_share_merchant_smallbiz", self.cc_share_merchant_smallbiz),
            ("cc_share_merchant_hnw", self.cc_share_merchant_hnw),
            ("autopay_full_p", self.autopay_full_p),
            ("autopay_min_p", self.autopay_min_p),
            ("manual_pay_full_p", self.manual_pay_full_p),
            ("manual_pay_partial_p", self.manual_pay_partial_p),
            ("manual_pay_min_p", self.manual_pay_min_p),
            ("manual_miss_p", self.manual_miss_p),
            ("late_payment_p", self.late_payment_p),
            ("refund_p", self.refund_p),
            ("chargeback_p", self.chargeback_p),
        )
        for name, value in probability_fields:
            require_float_between(name, value, 0.0, 1.0)

        require_float_gt("apr_median", self.apr_median, 0.0)
        require_float_ge("apr_sigma", self.apr_sigma, 0.0)
        require_float_gt("apr_min", self.apr_min, 0.0)
        require_float_gt("apr_max", self.apr_max, 0.0)
        if float(self.apr_max) < float(self.apr_min):
            raise ValueError("apr_max must be >= apr_min")

        require_float_gt("limit_sigma", self.limit_sigma, 0.0)

        limit_fields: tuple[tuple[str, float], ...] = (
            ("limit_student_median", self.limit_student_median),
            ("limit_salaried_median", self.limit_salaried_median),
            ("limit_retired_median", self.limit_retired_median),
            ("limit_freelancer_median", self.limit_freelancer_median),
            ("limit_smallbiz_median", self.limit_smallbiz_median),
            ("limit_hnw_median", self.limit_hnw_median),
        )
        for name, value in limit_fields:
            require_float_gt(name, value, 0.0)

        require_int_ge("cycle_day_min", self.cycle_day_min, 1)
        require_int_ge("cycle_day_max", self.cycle_day_max, self.cycle_day_min)
        require_int_ge("grace_days", self.grace_days, 1)

        require_float_between("min_payment_pct", self.min_payment_pct, 0.0, 1.0)
        require_float_ge("min_payment_dollars", self.min_payment_dollars, 0.0)

        require_float_gt("partial_beta_a", self.partial_beta_a, 0.0)
        require_float_gt("partial_beta_b", self.partial_beta_b, 0.0)

        require_int_ge("late_days_min", self.late_days_min, 0)
        require_int_ge("late_days_max", self.late_days_max, self.late_days_min)
        require_float_ge("late_fee_amount", self.late_fee_amount, 0.0)

        require_int_ge("refund_delay_days_min", self.refund_delay_days_min, 0)
        require_int_ge(
            "refund_delay_days_max",
            self.refund_delay_days_max,
            self.refund_delay_days_min,
        )

        require_int_ge("chargeback_delay_days_min", self.chargeback_delay_days_min, 0)
        require_int_ge(
            "chargeback_delay_days_max",
            self.chargeback_delay_days_max,
            self.chargeback_delay_days_min,
        )


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
    merchants: MerchantsConfig = field(default_factory=MerchantsConfig)
    family: FamilyConfig = field(default_factory=FamilyConfig)
    credit: CreditConfig = field(default_factory=CreditConfig)

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
    cfg.merchants.validate()
    cfg.family.validate()
    cfg.credit.validate()


def default_config() -> GenerationConfig:
    cfg = GenerationConfig()
    cfg.validate()
    return cfg
