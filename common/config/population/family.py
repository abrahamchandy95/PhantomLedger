from dataclasses import dataclass, field

from common.validate import ge, validate_metadata


@dataclass(frozen=True, slots=True)
class Households:
    # --- household structure ---
    single_p: float = field(default=0.29, metadata={"between": (0.0, 1.0)})
    zipf_alpha: float = field(default=2.2, metadata={"gt": 0.0})
    max_size: int = field(default=6, metadata={"ge": 2})
    spouse_p: float = field(default=0.62, metadata={"between": (0.0, 1.0)})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class Dependents:
    # --- dependency structure ---
    student_dependent_p: float = field(
        default=0.65,
        metadata={"between": (0.0, 1.0)},
    )
    student_coresides_p: float = field(
        default=0.35,
        metadata={"between": (0.0, 1.0)},
    )
    two_parent_p: float = field(default=0.70, metadata={"between": (0.0, 1.0)})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class Allowances:
    # --- allowance process ---
    enabled: bool = True
    weekly_p: float = field(default=0.60, metadata={"between": (0.0, 1.0)})
    pareto_xm: float = field(default=15.0, metadata={"gt": 0.0})
    pareto_alpha: float = field(default=2.2, metadata={"gt": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class Tuition:
    # --- tuition process ---
    enabled: bool = True
    p: float = field(default=0.55, metadata={"between": (0.0, 1.0)})
    inst_min: int = field(default=4, metadata={"ge": 1})
    inst_max: int = 5
    mu: float = 8.7
    sigma: float = field(default=0.35, metadata={"ge": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)
        ge("inst_max", self.inst_max, self.inst_min)


@dataclass(frozen=True, slots=True)
class RetireeSupport:
    # --- support to retirees ---
    enabled: bool = True
    has_child_p: float = field(default=0.35, metadata={"between": (0.0, 1.0)})
    support_p: float = field(default=0.35, metadata={"between": (0.0, 1.0)})
    coresides_p: float = field(default=0.12, metadata={"between": (0.0, 1.0)})
    pareto_xm: float = field(default=25.0, metadata={"gt": 0.0})
    pareto_alpha: float = field(default=2.4, metadata={"gt": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class Spouses:
    # --- spouse-to-spouse transfers ---
    # ~60% of couples have at least partially separate accounts
    # and need inter-spouse transfers (Census 2023, Bankrate 2024).
    # Couples with fully joint accounts use the shared account directly.
    enabled: bool = True
    separate_accounts_p: float = field(
        default=0.60,
        metadata={"between": (0.0, 1.0)},
    )
    txns_per_month_min: int = field(default=2, metadata={"ge": 1})
    txns_per_month_max: int = 6

    # Breadwinner asymmetry: 0.5 = symmetric, higher = more flow
    # from higher earner to lower earner. Pew 2023: 55% of marriages
    # have a husband as primary/sole breadwinner, 29% egalitarian,
    # 16% wife breadwinner. This skews transfer direction.
    breadwinner_flow_p: float = field(
        default=0.65,
        metadata={"between": (0.0, 1.0)},
    )
    transfer_median: float = field(default=85.0, metadata={"gt": 0.0})
    transfer_sigma: float = field(default=0.9, metadata={"ge": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)
        ge("txns_per_month_max", self.txns_per_month_max, self.txns_per_month_min)


@dataclass(frozen=True, slots=True)
class ParentGifts:
    # --- parent-to-adult-child gifts ---
    # 50% of parents financially support adult children (Savings.com 2025).
    # Downward transfers outnumber upward 7:1 (HRS longitudinal data).
    # Average support: ~$1,300/month among those who give.
    enabled: bool = True
    p: float = field(default=0.12, metadata={"between": (0.0, 1.0)})
    pareto_xm: float = field(default=75.0, metadata={"gt": 0.0})
    pareto_alpha: float = field(default=1.6, metadata={"gt": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class SiblingTransfers:
    # --- sibling transfers ---
    # Pew 2023 found only 24% of U.S. adults say siblings have at least a fair
    # amount of responsibility to provide financial assistance to one another,
    # so we keep sibling support relatively infrequent. Commercial 2025 surveys
    # suggest family/friends borrowing is often in the low hundreds, while
    # broader family-loan averages are skewed upward by large one-off loans.
    # We therefore anchor the recurring sibling-transfer process at a modest
    # low-hundreds median with a wide spread for occasional larger support.
    enabled: bool = True
    active_p: float = field(default=0.15, metadata={"between": (0.0, 1.0)})
    monthly_p: float = field(default=0.18, metadata={"between": (0.0, 1.0)})
    median: float = field(default=120.0, metadata={"gt": 0.0})
    sigma: float = field(default=0.90, metadata={"ge": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class GrandparentGifts:
    # --- grandparent gifts ---
    # 38-45% of 50+ households send money to younger generations (EBRI 2015).
    # Average ~$350/month among givers. We route from retired personas
    # to their children's children (implicit grandchildren).
    enabled: bool = True
    p: float = field(default=0.08, metadata={"between": (0.0, 1.0)})
    median: float = field(default=150.0, metadata={"gt": 0.0})
    sigma: float = field(default=0.70, metadata={"ge": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class Inheritance:
    # --- inheritance (rare lump-sum) ---
    # 26% of Americans expect to leave inheritance (NW Mutual 2024).
    # Per 180-day window, ~0.15% of retirees trigger a transfer event.
    # Amounts are large and follow a heavy-tailed distribution.
    enabled: bool = True
    event_p: float = field(default=0.0015, metadata={"between": (0.0, 1.0)})
    median: float = field(default=25000.0, metadata={"gt": 0.0})
    sigma: float = field(default=1.0, metadata={"ge": 0.0})

    def __post_init__(self) -> None:
        validate_metadata(self)


@dataclass(frozen=True, slots=True)
class Routing:
    # --- external family probability ---
    # Probability that a family counterparty banks at a different institution.
    # Co-residing families overwhelmingly share a bank; non-co-residing
    # adult children/siblings are more likely to bank elsewhere.
    # Blended estimate from FDIC 2023 survey + market fragmentation data.
    external_p: float = field(default=0.18, metadata={"between": (0.0, 1.0)})

    def __post_init__(self) -> None:
        validate_metadata(self)
