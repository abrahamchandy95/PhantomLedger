from dataclasses import dataclass

from common.config import Window
from common.random import Rng
from common.transactions import Transaction
from common.validate import (
    between,
    ge,
    gt,
)
import entities.models as entity_models
from transfers.factory import TransactionFactory


@dataclass(frozen=True, slots=True)
class Terms:
    """The mathematical constraints and rules set by the issuing bank."""

    grace_days: int = 25
    min_payment_pct: float = 0.02
    min_payment_dollars: float = 25.0
    late_fee: float = 32.0

    refund_delay_min: int = 1
    refund_delay_max: int = 14
    chargeback_delay_min: int = 7
    chargeback_delay_max: int = 45

    def __post_init__(self) -> None:
        ge("grace_days", self.grace_days, 1)
        between("min_payment_pct", self.min_payment_pct, 0.0, 1.0)
        ge("min_payment_dollars", self.min_payment_dollars, 0.0)
        ge("late_fee", self.late_fee, 0.0)

        ge("refund_delay_min", self.refund_delay_min, 0)
        ge("refund_delay_max", self.refund_delay_max, self.refund_delay_min)
        ge("chargeback_delay_min", self.chargeback_delay_min, 0)
        ge("chargeback_delay_max", self.chargeback_delay_max, self.chargeback_delay_min)


@dataclass(frozen=True, slots=True)
class Habits:
    """The statistical probabilities of how a person interacts with the card."""

    pay_full_p: float = 0.35
    pay_partial_p: float = 0.30
    pay_min_p: float = 0.25
    miss_p: float = 0.10

    partial_beta_a: float = 2.0
    partial_beta_b: float = 5.0

    late_payment_p: float = 0.08
    late_days_min: int = 1
    late_days_max: int = 20

    refund_p: float = 0.006
    chargeback_p: float = 0.001

    def __post_init__(self) -> None:
        probability_fields: tuple[tuple[str, float], ...] = (
            ("pay_full_p", self.pay_full_p),
            ("pay_partial_p", self.pay_partial_p),
            ("pay_min_p", self.pay_min_p),
            ("miss_p", self.miss_p),
            ("late_payment_p", self.late_payment_p),
            ("refund_p", self.refund_p),
            ("chargeback_p", self.chargeback_p),
        )
        for name, value in probability_fields:
            between(name, value, 0.0, 1.0)

        gt("partial_beta_a", self.partial_beta_a, 0.0)
        gt("partial_beta_b", self.partial_beta_b, 0.0)

        ge("late_days_min", self.late_days_min, 0)
        ge("late_days_max", self.late_days_max, self.late_days_min)


DEFAULT_TERMS = Terms()
DEFAULT_HABITS = Habits()


@dataclass(frozen=True, slots=True)
class Request:
    """The required inputs to generate a credit card transaction scenario."""

    window: Window
    rng: Rng
    base_seed: int
    cards: entity_models.CreditCards
    txns: list[Transaction]
    primary_accounts: dict[str, str]
    issuer_acct: str
    txf: TransactionFactory
