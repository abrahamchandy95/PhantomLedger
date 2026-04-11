from dataclasses import dataclass

from common.validate import (
    between,
    gt,
    ge,
)


@dataclass(frozen=True, slots=True)
class Family:
    # --- household structure ---
    single_p: float = 0.29
    zipf_alpha: float = 2.2
    max_size: int = 6

    spouse_p: float = 0.62

    # --- dependency structure ---
    student_dependent_p: float = 0.65
    student_coresides_p: float = 0.35
    two_parent_p: float = 0.70

    # --- allowance process ---
    allowance_enabled: bool = True
    allowance_weekly_p: float = 0.60
    allowance_pareto_xm: float = 15.0
    allowance_pareto_alpha: float = 2.2

    # --- tuition process ---
    tuition_enabled: bool = True
    tuition_p: float = 0.55
    tuition_inst_min: int = 4
    tuition_inst_max: int = 5
    tuition_mu: float = 8.7
    tuition_sigma: float = 0.35

    # --- support to retirees ---
    retiree_support_enabled: bool = True
    retiree_has_child_p: float = 0.35
    retiree_support_p: float = 0.35
    retiree_coresides_p: float = 0.12
    retiree_support_pareto_xm: float = 25.0
    retiree_support_pareto_alpha: float = 2.4

    # --- spouse-to-spouse transfers ---
    # ~60% of couples have at least partially separate accounts
    # and need inter-spouse transfers (Census 2023, Bankrate 2024).
    # Couples with fully joint accounts use the shared account directly.
    spouse_transfer_enabled: bool = True
    spouse_separate_accounts_p: float = 0.60
    spouse_txns_per_month_min: int = 2
    spouse_txns_per_month_max: int = 6

    # Breadwinner asymmetry: 0.5 = symmetric, higher = more flow
    # from higher earner to lower earner. Pew 2023: 55% of marriages
    # have a husband as primary/sole breadwinner, 29% egalitarian,
    # 16% wife breadwinner. This skews transfer direction.
    spouse_breadwinner_flow_p: float = 0.65

    spouse_transfer_median: float = 85.0
    spouse_transfer_sigma: float = 0.9

    # --- parent-to-adult-child gifts ---
    # 50% of parents financially support adult children (Savings.com 2025).
    # Downward transfers outnumber upward 7:1 (HRS longitudinal data).
    # Average support: ~$1,300/month among those who give.
    parent_gift_enabled: bool = True
    parent_gift_p: float = 0.12
    parent_gift_pareto_xm: float = 75.0
    parent_gift_pareto_alpha: float = 1.6

    # --- sibling transfers ---
    # Pew 2023 found only 24% of U.S. adults say siblings have at least a fair
    # amount of responsibility to provide financial assistance to one another,
    # so we keep sibling support relatively infrequent. Commercial 2025 surveys
    # suggest family/friends borrowing is often in the low hundreds, while
    # broader family-loan averages are skewed upward by large one-off loans.
    # We therefore anchor the recurring sibling-transfer process at a modest
    # low-hundreds median with a wide spread for occasional larger support.
    sibling_transfer_enabled: bool = True
    sibling_active_p: float = 0.15
    sibling_monthly_p: float = 0.18
    sibling_median: float = 120.0
    sibling_sigma: float = 0.90
    # --- grandparent gifts ---
    # 38-45% of 50+ households send money to younger generations (EBRI 2015).
    # Average ~$350/month among givers. We route from retired personas
    # to their children's children (implicit grandchildren).
    grandparent_gift_enabled: bool = True
    grandparent_gift_p: float = 0.08
    grandparent_gift_median: float = 150.0
    grandparent_gift_sigma: float = 0.70

    # --- inheritance (rare lump-sum) ---
    # 26% of Americans expect to leave inheritance (NW Mutual 2024).
    # Per 180-day window, ~0.15% of retirees trigger a transfer event.
    # Amounts are large and follow a heavy-tailed distribution.
    inheritance_enabled: bool = True
    inheritance_event_p: float = 0.0015
    inheritance_median: float = 25000.0
    inheritance_sigma: float = 1.0
    # --- external family probability ---
    # Probability that a family counterparty banks at a different institution.
    # Co-residing families overwhelmingly share a bank; non-co-residing
    # adult children/siblings are more likely to bank elsewhere.
    # Blended estimate from FDIC 2023 survey + market fragmentation data.
    external_family_p: float = 0.18

    def __post_init__(self) -> None:
        between("single_p", self.single_p, 0.0, 1.0)
        gt("zipf_alpha", self.zipf_alpha, 0.0)
        ge("max_size", self.max_size, 2)

        between("spouse_p", self.spouse_p, 0.0, 1.0)

        between("student_dependent_p", self.student_dependent_p, 0.0, 1.0)
        between("student_coresides_p", self.student_coresides_p, 0.0, 1.0)
        between("two_parent_p", self.two_parent_p, 0.0, 1.0)

        between("allowance_weekly_p", self.allowance_weekly_p, 0.0, 1.0)
        gt("allowance_pareto_xm", self.allowance_pareto_xm, 0.0)
        gt("allowance_pareto_alpha", self.allowance_pareto_alpha, 0.0)

        between("tuition_p", self.tuition_p, 0.0, 1.0)
        ge("tuition_inst_min", self.tuition_inst_min, 1)
        ge("tuition_inst_max", self.tuition_inst_max, self.tuition_inst_min)

        between("retiree_has_child_p", self.retiree_has_child_p, 0.0, 1.0)
        between("retiree_support_p", self.retiree_support_p, 0.0, 1.0)
        between("retiree_coresides_p", self.retiree_coresides_p, 0.0, 1.0)
        gt("retiree_support_pareto_xm", self.retiree_support_pareto_xm, 0.0)
        gt("retiree_support_pareto_alpha", self.retiree_support_pareto_alpha, 0.0)

        between("spouse_separate_accounts_p", self.spouse_separate_accounts_p, 0.0, 1.0)
        ge("spouse_txns_per_month_min", self.spouse_txns_per_month_min, 1)
        ge(
            "spouse_txns_per_month_max",
            self.spouse_txns_per_month_max,
            self.spouse_txns_per_month_min,
        )
        between("spouse_breadwinner_flow_p", self.spouse_breadwinner_flow_p, 0.0, 1.0)
        gt("spouse_transfer_median", self.spouse_transfer_median, 0.0)
        ge("spouse_transfer_sigma", self.spouse_transfer_sigma, 0.0)

        between("parent_gift_p", self.parent_gift_p, 0.0, 1.0)
        gt("parent_gift_pareto_xm", self.parent_gift_pareto_xm, 0.0)
        gt("parent_gift_pareto_alpha", self.parent_gift_pareto_alpha, 0.0)
        between("sibling_active_p", self.sibling_active_p, 0.0, 1.0)
        between("sibling_monthly_p", self.sibling_monthly_p, 0.0, 1.0)
        gt("sibling_median", self.sibling_median, 0.0)
        ge("sibling_sigma", self.sibling_sigma, 0.0)

        between("grandparent_gift_p", self.grandparent_gift_p, 0.0, 1.0)
        gt("grandparent_gift_median", self.grandparent_gift_median, 0.0)
        ge("grandparent_gift_sigma", self.grandparent_gift_sigma, 0.0)

        between("inheritance_event_p", self.inheritance_event_p, 0.0, 1.0)
        gt("inheritance_median", self.inheritance_median, 0.0)
        ge("inheritance_sigma", self.inheritance_sigma, 0.0)
        between("external_family_p", self.external_family_p, 0.0, 1.0)
