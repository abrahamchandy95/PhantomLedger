from datetime import datetime, timedelta
from math import isfinite

import numpy as np

from common.channels import CC_REFUND, CC_CHARGEBACK
from common.random import Rng
from common.transactions import Transaction
from transfers.factory import TransactionDraft, TransactionFactory

from .params import Habits, Terms


def calculate_interval_days(a: datetime, b: datetime) -> float:
    """Calculates the fractional number of days between two timestamps."""
    return max(0.0, (b - a).total_seconds() / 86400.0)


def _balance_delta(card: str, txn: Transaction) -> float:
    """
    Card balance convention:
      - if card is src: card is charged => balance increases (more debt)
      - if card is dst: card is credited => balance decreases (less debt)
    """
    if txn.source == card:
        return -float(txn.amount)
    if txn.target == card:
        return +float(txn.amount)
    return 0.0


def calculate_avg_balance(
    card: str,
    balance_at_start: float,
    events: list[Transaction],
    t0: datetime,
    t1: datetime,
) -> tuple[float, float]:
    """
    Piecewise-constant integration of balance over [t0, t1).
    Returns: (average_balance_over_interval, ending_balance)
    """
    if t1 <= t0:
        return balance_at_start, balance_at_start

    balance = float(balance_at_start)
    integral = 0.0
    prev = t0

    for event in events:
        ts = event.timestamp
        if ts < t0 or ts >= t1:
            continue

        dt = calculate_interval_days(prev, ts)
        integral += balance * dt

        balance += _balance_delta(card, event)
        prev = ts

    dt_last = calculate_interval_days(prev, t1)
    integral += balance * dt_last

    total_days = calculate_interval_days(t0, t1)
    if total_days <= 0.0:
        return balance, balance

    return integral / total_days, balance


def calculate_min_due(terms: Terms, statement_abs: float) -> float:
    return max(
        float(terms.min_payment_dollars),
        float(terms.min_payment_pct) * float(statement_abs),
    )


def sample_manual_payment(
    habits: Habits,
    rng: Rng,
    statement_abs: float,
    min_due: float,
) -> float:
    """Stochastic manual-payment model: miss / min / partial / full."""
    u = float(rng.float())

    miss = float(habits.miss_p)
    pay_min = float(habits.pay_min_p)
    pay_part = float(habits.pay_partial_p)
    pay_full = float(habits.pay_full_p)

    total = miss + pay_min + pay_part + pay_full
    if total <= 0.0:
        return 0.0

    miss /= total
    pay_min /= total
    pay_part /= total
    pay_full /= total

    if u < miss:
        return 0.0
    u -= miss

    if u < pay_min:
        return float(min_due)
    u -= pay_min

    if u < pay_part:
        remaining = max(0.0, float(statement_abs) - float(min_due))
        if remaining <= 1e-9:
            return float(min_due)

        frac = float(
            rng.gen.beta(float(habits.partial_beta_a), float(habits.partial_beta_b))
        )
        return float(min_due) + frac * remaining

    return float(statement_abs)


def sample_payment_time(
    habits: Habits,
    rng: Rng,
    due: datetime,
    *,
    is_autopay: bool,
) -> datetime:
    """
    Autopay occurs precisely on the due date.
    Manual payment may be early/on-time or late based on policy probabilities.
    """
    if is_autopay:
        return due + timedelta(hours=12)

    if rng.coin(float(habits.late_payment_p)):
        delay = rng.int(int(habits.late_days_min), int(habits.late_days_max) + 1)
        return due + timedelta(
            days=delay,
            hours=rng.int(9, 21),
            minutes=rng.int(0, 60),
        )

    early = rng.int(0, 4)
    return (
        due
        - timedelta(days=early)
        + timedelta(
            hours=rng.int(9, 21),
            minutes=rng.int(0, 60),
        )
    )


def sample_merchant_credit(
    terms: Terms,
    habits: Habits,
    gen: np.random.Generator,
    *,
    purchase: Transaction,
    end_excl: datetime,
    txf: TransactionFactory,
) -> Transaction | None:
    """
    Probabilistically schedule either a refund, a chargeback, or nothing.

    Refunds and chargebacks flow from the merchant that originally received
    the purchase back to the card that paid for it. This matches how card
    networks actually settle reversals: the acquiring merchant (or their
    settlement bank) credits the cardholder's account.

    When the merchant is an internal account at our institution, the refund
    posts as an on-us book transfer. When the merchant is external, it posts
    as an inbound external credit. Either way, no synthetic "refund source"
    counterparty is required.
    """
    u = float(gen.random())

    if u < float(habits.refund_p):
        delay = int(
            gen.integers(
                int(terms.refund_delay_min),
                int(terms.refund_delay_max) + 1,
            )
        )
        ts = purchase.timestamp + timedelta(days=delay, hours=int(gen.integers(9, 21)))
        if ts >= end_excl:
            return None

        return txf.make(
            TransactionDraft(
                source=purchase.target,
                destination=purchase.source,
                amount=float(purchase.amount),
                timestamp=ts,
                channel=CC_REFUND,
            )
        )

    u2 = float(gen.random())
    if u2 < float(habits.chargeback_p):
        delay = int(
            gen.integers(
                int(terms.chargeback_delay_min),
                int(terms.chargeback_delay_max) + 1,
            )
        )
        ts = purchase.timestamp + timedelta(days=delay, hours=int(gen.integers(9, 21)))
        if ts >= end_excl:
            return None

        return txf.make(
            TransactionDraft(
                source=purchase.target,
                destination=purchase.source,
                amount=float(purchase.amount),
                timestamp=ts,
                channel=CC_CHARGEBACK,
            )
        )

    return None


def get_billable_interest(amount: float) -> float | None:
    """Filters out micro-pennies and invalid floats."""
    if not isfinite(amount) or amount <= 0.01:
        return None
    return round(float(amount), 2)
