from dataclasses import dataclass

from common.validate import (
    require_float_between,
    require_float_gt,
    require_int_ge,
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
            "single_household_frac",
            self.single_household_frac,
            0.0,
            1.0,
        )
        require_float_gt("household_zipf_alpha", self.household_zipf_alpha, 0.0)
        require_int_ge("household_max_size", self.household_max_size, 2)

        require_float_between("spouse_pair_p", self.spouse_pair_p, 0.0, 1.0)

        require_float_between(
            "student_dependent_p",
            self.student_dependent_p,
            0.0,
            1.0,
        )
        require_float_between(
            "co_reside_student_p",
            self.co_reside_student_p,
            0.0,
            1.0,
        )
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
            "retiree_has_adult_child_p",
            self.retiree_has_adult_child_p,
            0.0,
            1.0,
        )
        require_float_between("retiree_support_p", self.retiree_support_p, 0.0, 1.0)
        require_float_between(
            "retiree_support_co_reside_p",
            self.retiree_support_co_reside_p,
            0.0,
            1.0,
        )
        require_float_gt(
            "retiree_support_pareto_xm",
            self.retiree_support_pareto_xm,
            0.0,
        )
        require_float_gt(
            "retiree_support_pareto_alpha",
            self.retiree_support_pareto_alpha,
            0.0,
        )
