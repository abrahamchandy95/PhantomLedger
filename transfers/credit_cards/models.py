from dataclasses import dataclass

from common.config import WindowConfig
from common.random import Rng
from common.transactions import Transaction
from common.validate import (
    require_float_between,
    require_float_ge,
    require_float_gt,
    require_int_ge,
)
from entities.credit_cards import CreditCardData
from transfers.txns import TxnFactory


@dataclass(frozen=True, slots=True)
class CreditLifecyclePolicy:
    grace_days: int = 25

    min_payment_pct: float = 0.02
    min_payment_dollars: float = 25.0

    manual_pay_full_p: float = 0.35
    manual_pay_partial_p: float = 0.30
    manual_pay_min_p: float = 0.25
    manual_miss_p: float = 0.10

    partial_beta_a: float = 2.0
    partial_beta_b: float = 5.0

    late_payment_p: float = 0.08
    late_days_min: int = 1
    late_days_max: int = 20
    late_fee_amount: float = 32.0

    refund_p: float = 0.006
    refund_delay_days_min: int = 1
    refund_delay_days_max: int = 14

    chargeback_p: float = 0.001
    chargeback_delay_days_min: int = 7
    chargeback_delay_days_max: int = 45

    def validate(self) -> None:
        probability_fields: tuple[tuple[str, float], ...] = (
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


DEFAULT_CREDIT_LIFECYCLE_POLICY = CreditLifecyclePolicy()


@dataclass(frozen=True, slots=True)
class CreditLifecycleRequest:
    window: WindowConfig
    rng: Rng
    base_seed: int
    cards: CreditCardData
    existing_txns: list[Transaction]
    primary_acct_for_person: dict[str, str]
    issuer_acct: str
    txf: TxnFactory
