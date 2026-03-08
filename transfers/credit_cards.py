import calendar
from datetime import datetime, timedelta
from math import isfinite

import numpy as np

from common.config import CreditConfig, WindowConfig
from common.rng import Rng
from common.seeding import derived_seed
from common.types import Txn
from entities.credit_cards import CreditCardData
from transfers.txns import TxnFactory, TxnSpec


def _days_frac(a: datetime, b: datetime) -> float:
    return max(0.0, (b - a).total_seconds() / 86400.0)


def _safe_month_day(year: int, month: int, day: int) -> int:
    last = calendar.monthrange(year, month)[1]
    return min(day, last)


def _iter_cycle_closes(
    start: datetime,
    end_excl: datetime,
    cycle_day: int,
) -> list[datetime]:
    """
    Monthly close timestamps inside [start, end_excl).
    Close time is fixed at 23:30 on the close day.
    """
    out: list[datetime] = []

    y = start.year
    m = start.month

    while True:
        d = _safe_month_day(y, m, cycle_day)
        close = datetime(y, m, d, 23, 30, 0)

        if close < start:
            if m == 12:
                y, m = y + 1, 1
            else:
                m += 1
            continue

        if close >= end_excl:
            break

        out.append(close)

        if m == 12:
            y, m = y + 1, 1
        else:
            m += 1

    return out


def _effective_due_cutoff(due: datetime) -> datetime:
    """
    Approximate 'received by due date' logic.

    CFPB guidance says a payment generally cannot be treated as late if
    received by 5 p.m. on the due date, or the next business day if the
    due date is on a weekend/holiday. We model the weekend part here.
    """
    cutoff = due

    # roll Saturday/Sunday to Monday, preserving local cut-off time
    while cutoff.weekday() >= 5:
        cutoff += timedelta(days=1)

    return cutoff.replace(second=0, microsecond=0)


def _payment_received_on_time(payment_ts: datetime, due: datetime) -> bool:
    return payment_ts <= _effective_due_cutoff(due)


def _balance_delta_for_card(card: str, t: Txn) -> float:
    """
    Card balance convention:
      - if card is src: card is charged => balance decreases (more debt)
      - if card is dst: card is credited => balance increases (less debt)
    """
    if t.src_acct == card:
        return -float(t.amount)
    if t.dst_acct == card:
        return +float(t.amount)
    return 0.0


def _integrated_avg_balance(
    card: str,
    b0: float,
    events: list[Txn],
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
        return b0, b0

    bal = float(b0)
    acc = 0.0
    prev = t0

    for e in events:
        ts = e.ts
        if ts < t0 or ts >= t1:
            continue

        dt = _days_frac(prev, ts)
        acc += bal * dt

        bal += _balance_delta_for_card(card, e)
        prev = ts

    dt_last = _days_frac(prev, t1)
    acc += bal * dt_last

    T = _days_frac(t0, t1)
    if T <= 0.0:
        return bal, bal

    return acc / T, bal


def _min_payment(ccfg: CreditConfig, statement_abs: float) -> float:
    return max(
        float(ccfg.min_payment_dollars),
        float(ccfg.min_payment_pct) * float(statement_abs),
    )


def _choose_manual_payment_amount(
    ccfg: CreditConfig,
    rng: Rng,
    statement_abs: float,
    min_due: float,
) -> float:
    """
    Stochastic manual-payment model:
      miss / min / partial / full
    """
    u = float(rng.float())

    miss = float(ccfg.manual_miss_p)
    pay_min = float(ccfg.manual_pay_min_p)
    pay_part = float(ccfg.manual_pay_partial_p)
    pay_full = float(ccfg.manual_pay_full_p)

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
            rng.gen.beta(float(ccfg.partial_beta_a), float(ccfg.partial_beta_b))
        )
        return float(min_due) + frac * remaining

    return float(statement_abs)


def _payment_timestamp(
    ccfg: CreditConfig,
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

    if rng.coin(float(ccfg.late_payment_p)):
        delay = rng.int(int(ccfg.late_days_min), int(ccfg.late_days_max) + 1)
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


def _refund_src_for(card: str, idx: int) -> str:
    """
    External source account used for credits back to the card.
    Starts with X so balance logic treats it as external->internal.
    """
    return f"XREFUND_{card}_{idx:04d}"


def _sample_credit_event_for_purchase(
    ccfg: CreditConfig,
    g: np.random.Generator,
    *,
    card: str,
    credit_idx: int,
    purchase: Txn,
    end_excl: datetime,
    txf: TxnFactory,
) -> Txn | None:
    """
    For a single purchase, probabilistically schedule either:
      - a refund, with probability refund_p
      - else a chargeback, with probability chargeback_p
      - else nothing

    This keeps the model mathematical and probabilistic.
    """
    u = float(g.random())
    if u < float(ccfg.refund_p):
        delay = int(
            g.integers(
                int(ccfg.refund_delay_days_min),
                int(ccfg.refund_delay_days_max) + 1,
            )
        )
        ts = purchase.ts + timedelta(days=delay, hours=int(g.integers(9, 21)))
        if ts >= end_excl:
            return None

        return txf.make(
            TxnSpec(
                src=_refund_src_for(card, credit_idx),
                dst=card,
                amt=float(purchase.amount),
                ts=ts,
                channel="cc_refund",
            )
        )

    u2 = float(g.random())
    if u2 < float(ccfg.chargeback_p):
        delay = int(
            g.integers(
                int(ccfg.chargeback_delay_days_min),
                int(ccfg.chargeback_delay_days_max) + 1,
            )
        )
        ts = purchase.ts + timedelta(days=delay, hours=int(g.integers(9, 21)))
        if ts >= end_excl:
            return None

        return txf.make(
            TxnSpec(
                src=_refund_src_for(card, credit_idx),
                dst=card,
                amt=float(purchase.amount),
                ts=ts,
                channel="cc_chargeback",
            )
        )

    return None


def generate_credit_card_lifecycle_txns(
    window: WindowConfig,
    ccfg: CreditConfig,
    rng: Rng,
    *,
    base_seed: int,
    cards: CreditCardData,
    existing_txns: list[Txn],
    primary_acct_for_person: dict[str, str],
    issuer_acct: str,
    txf: TxnFactory,
) -> list[Txn]:
    """
    Reads existing card_purchase txns and generates:
      - refunds / chargebacks (external -> card)
      - interest (card -> issuer)
      - late fees (card -> issuer)
      - payments (deposit -> card)

    Model notes:
      - Purchases create negative card balance (debt).
      - Credits/payments reduce that debt.
      - Purchase grace period is preserved only if the statement balance
        is paid in full by the due date.
    """
    if not ccfg.enable_credit_cards or not cards.card_accounts:
        return []

    start = window.start_date()
    end_excl = start + timedelta(days=int(window.days))

    card_set = set(cards.card_accounts)

    purchases_by_card: dict[str, list[Txn]] = {c: [] for c in cards.card_accounts}
    for t in existing_txns:
        if t.channel != "card_purchase":
            continue
        if t.src_acct in card_set and start <= t.ts < end_excl:
            purchases_by_card[t.src_acct].append(t)

    out: list[Txn] = []

    # deterministic generator for lifecycle-side credit events
    g_global = np.random.default_rng(derived_seed(base_seed, "cc_lifecycle"))

    for card, purchases in purchases_by_card.items():
        if not purchases:
            continue

        purchases.sort(key=lambda x: x.ts)

        owner = cards.owner_for_card.get(card)
        if owner is None:
            continue

        pay_from = primary_acct_for_person.get(owner)
        if pay_from is None:
            continue

        apr = float(cards.apr_by_card.get(card, float(ccfg.apr_median)))
        cycle_day = int(cards.cycle_day_by_card.get(card, 15))
        autopay_mode = int(cards.autopay_mode_by_card.get(card, 0))

        closes = _iter_cycle_closes(start, end_excl, cycle_day)
        if not closes:
            continue

        # negative balance means debt
        bal = 0.0
        in_grace = True

        scheduled_credits: list[Txn] = []
        credit_idx = 0

        purchase_i = 0
        last_close = start

        for close in closes:
            cycle_start = last_close
            cycle_end = close

            cycle_events: list[Txn] = []

            while purchase_i < len(purchases) and purchases[purchase_i].ts < cycle_end:
                pt = purchases[purchase_i]
                cycle_events.append(pt)

                credit_idx += 1
                credit_txn = _sample_credit_event_for_purchase(
                    ccfg,
                    g_global,
                    card=card,
                    credit_idx=credit_idx,
                    purchase=pt,
                    end_excl=end_excl,
                    txf=txf,
                )
                if credit_txn is not None:
                    scheduled_credits.append(credit_txn)
                    out.append(credit_txn)

                purchase_i += 1

            if scheduled_credits:
                keep: list[Txn] = []
                for ct in scheduled_credits:
                    if cycle_start <= ct.ts < cycle_end:
                        cycle_events.append(ct)
                    else:
                        keep.append(ct)
                scheduled_credits = keep

            cycle_events.sort(key=lambda x: x.ts)

            avg_bal, bal_end = _integrated_avg_balance(
                card,
                bal,
                cycle_events,
                cycle_start,
                cycle_end,
            )
            bal = bal_end

            if not in_grace:
                interval_days = _days_frac(cycle_start, cycle_end)
                debt_avg = max(0.0, -float(avg_bal))
                interest = debt_avg * (float(apr) / 365.0) * float(interval_days)

                if isfinite(interest) and interest > 0.01:
                    interest = round(interest, 2)
                    interest_txn = txf.make(
                        TxnSpec(
                            src=card,
                            dst=issuer_acct,
                            amt=interest,
                            ts=close + timedelta(minutes=15),
                            channel="cc_interest",
                        )
                    )
                    out.append(interest_txn)
                    bal -= float(interest)

            statement_abs = max(0.0, -bal)
            if statement_abs <= 0.01:
                in_grace = True
                last_close = close
                continue

            min_due = round(_min_payment(ccfg, statement_abs), 2)
            due = close + timedelta(days=int(ccfg.grace_days), hours=17)

            if autopay_mode == 2:
                pay_amt = float(statement_abs)
                pay_ts = _payment_timestamp(ccfg, rng, due, is_autopay=True)
            elif autopay_mode == 1:
                pay_amt = float(min_due)
                pay_ts = _payment_timestamp(ccfg, rng, due, is_autopay=True)
            else:
                pay_amt = _choose_manual_payment_amount(
                    ccfg,
                    rng,
                    statement_abs,
                    min_due,
                )
                pay_ts = _payment_timestamp(ccfg, rng, due, is_autopay=False)

            pay_amt = round(max(0.0, float(pay_amt)), 2)

            paid_by_due = (pay_amt >= min_due - 1e-6) and _payment_received_on_time(
                pay_ts, due
            )
            paid_full_by_due = (
                pay_amt >= statement_abs - 1e-6
            ) and _payment_received_on_time(pay_ts, due)

            if pay_amt > 0.0 and pay_ts < end_excl:
                payment_txn = txf.make(
                    TxnSpec(
                        src=pay_from,
                        dst=card,
                        amt=pay_amt,
                        ts=pay_ts,
                        channel="cc_payment",
                    )
                )
                out.append(payment_txn)
                bal += float(pay_amt)

            if not paid_by_due and float(ccfg.late_fee_amount) > 0.0:
                fee_ts = min(
                    end_excl - timedelta(seconds=1),
                    _effective_due_cutoff(due) + timedelta(days=1, hours=10),
                )
                fee = round(float(ccfg.late_fee_amount), 2)
                fee_txn = txf.make(
                    TxnSpec(
                        src=card,
                        dst=issuer_acct,
                        amt=fee,
                        ts=fee_ts,
                        channel="cc_late_fee",
                    )
                )
                out.append(fee_txn)
                bal -= float(fee)

            in_grace = bool(paid_full_by_due)
            last_close = close

    out.sort(
        key=lambda t: (
            t.ts,
            t.src_acct,
            t.dst_acct,
            float(t.amount),
            t.channel or "",
        )
    )
    return out
