from dataclasses import dataclass

from common.validate import (
    require_float_between,
    require_float_ge,
    require_float_gt,
    require_int_ge,
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
