from datetime import datetime, timedelta
from math import isfinite

import numpy as np

from common.random import Rng
from common.transactions import Transaction
from transfers.txns import TxnFactory, TxnSpec

from .models import CreditLifecyclePolicy


def days_frac(a: datetime, b: datetime) -> float:
    return max(0.0, (b - a).total_seconds() / 86400.0)


def balance_delta_for_card(card: str, txn: Transaction) -> float:
    """
    Card balance convention:
      - if card is src: card is charged => balance decreases (more debt)
      - if card is dst: card is credited => balance increases (less debt)
    """
    if txn.source == card:
        return -float(txn.amount)
    if txn.target == card:
        return +float(txn.amount)
    return 0.0


def integrated_avg_balance(
    card: str,
    balance_at_start: float,
    events: list[Transaction],
    t0: datetime,
    t1: datetime,
) -> tuple[float, float]:
    """
    Piecewise-constant integration of balance over [t0, t1).

    avg_balance = (1 / T) * integral(balance(t) dt)

    Returns:
      (average_balance_over_interval, ending_balance_at_t1)
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

        dt = days_frac(prev, ts)
        integral += balance * dt

        balance += balance_delta_for_card(card, event)
        prev = ts

    dt_last = days_frac(prev, t1)
    integral += balance * dt_last

    total_days = days_frac(t0, t1)
    if total_days <= 0.0:
        return balance, balance

    return integral / total_days, balance


def min_payment(policy: CreditLifecyclePolicy, statement_abs: float) -> float:
    return max(
        float(policy.min_payment_dollars),
        float(policy.min_payment_pct) * float(statement_abs),
    )


def choose_manual_payment_amount(
    policy: CreditLifecyclePolicy,
    rng: Rng,
    statement_abs: float,
    min_due: float,
) -> float:
    """
    Stochastic manual-payment model:
      miss / min / partial / full
    """
    u = float(rng.float())

    miss = float(policy.manual_miss_p)
    pay_min = float(policy.manual_pay_min_p)
    pay_part = float(policy.manual_pay_partial_p)
    pay_full = float(policy.manual_pay_full_p)

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
            rng.gen.beta(float(policy.partial_beta_a), float(policy.partial_beta_b))
        )
        return float(min_due) + frac * remaining

    return float(statement_abs)


def payment_timestamp(
    policy: CreditLifecyclePolicy,
    rng: Rng,
    due: datetime,
    *,
    is_autopay: bool,
) -> datetime:
    """
    Autopay tends to occur on the due date.
    Manual payment may be early/on-time or late.
    """
    if is_autopay:
        return due + timedelta(hours=12)

    if rng.coin(float(policy.late_payment_p)):
        delay = rng.int(int(policy.late_days_min), int(policy.late_days_max) + 1)
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


def refund_src_for(card: str, idx: int) -> str:
    """
    External source account used for credits back to the card.
    Starts with X so balance logic treats it as external->internal.
    """
    return f"XREFUND_{card}_{idx:04d}"


def sample_credit_event_for_purchase(
    policy: CreditLifecyclePolicy,
    gen: np.random.Generator,
    *,
    card: str,
    credit_idx: int,
    purchase: Transaction,
    end_excl: datetime,
    txf: TxnFactory,
) -> Transaction | None:
    """
    For a single purchase, probabilistically schedule either:
      - a refund, with probability refund_p
      - else a chargeback, with probability chargeback_p
      - else nothing
    """
    u = float(gen.random())
    if u < float(policy.refund_p):
        delay = int(
            gen.integers(
                int(policy.refund_delay_days_min),
                int(policy.refund_delay_days_max) + 1,
            )
        )
        ts = purchase.timestamp + timedelta(days=delay, hours=int(gen.integers(9, 21)))
        if ts >= end_excl:
            return None

        return txf.make(
            TxnSpec(
                src=refund_src_for(card, credit_idx),
                dst=card,
                amt=float(purchase.amount),
                ts=ts,
                channel="cc_refund",
            )
        )

    u2 = float(gen.random())
    if u2 < float(policy.chargeback_p):
        delay = int(
            gen.integers(
                int(policy.chargeback_delay_days_min),
                int(policy.chargeback_delay_days_max) + 1,
            )
        )
        ts = purchase.timestamp + timedelta(days=delay, hours=int(gen.integers(9, 21)))
        if ts >= end_excl:
            return None

        return txf.make(
            TxnSpec(
                src=refund_src_for(card, credit_idx),
                dst=card,
                amt=float(purchase.amount),
                ts=ts,
                channel="cc_chargeback",
            )
        )

    return None


def finite_interest_amount(amount: float) -> float | None:
    if not isfinite(amount) or amount <= 0.01:
        return None
    return round(float(amount), 2)
