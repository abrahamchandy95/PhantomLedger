from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime, timedelta

import numpy as np

from common.channels import CC_INTEREST, CC_PAYMENT, CC_LATE_FEE, CARD_PURCHASE
from common.random import RngFactory
from common.transactions import Transaction
from transfers.factory import TransactionDraft

from .params import Habits, Request, Terms
from .schedule import calculate_cycle_closes, is_on_time, resolve_due_date
from .statement import (
    calculate_avg_balance,
    calculate_interval_days,
    calculate_min_due,
    get_billable_interest,
    sample_manual_payment,
    sample_merchant_credit,
    sample_payment_time,
)

_DEFAULT_CARD_APR = 0.22
_DEFAULT_CYCLE_DAY = 15


@dataclass(frozen=True, slots=True)
class _CardContext:
    """Holds the static evaluated parameters for a single card."""

    card: str
    pay_from: str
    apr: float
    cycle_day: int
    autopay_mode: int


@dataclass(slots=True)
class _CycleState:
    """Holds the mutable financial state of a card across billing cycles."""

    balance: float = 0.0
    in_grace: bool = True
    purchase_idx: int = 0
    credit_idx: int = 0
    scheduled_credits: list[Transaction] = field(default_factory=list)


def _group_purchases(
    request: Request,
    start: datetime,
    end_excl: datetime,
) -> dict[str, list[Transaction]]:
    """grouping of relevant card transactions."""
    card_set = set(request.cards.ids)
    grouped: dict[str, list[Transaction]] = defaultdict(list)

    for txn in request.txns:
        if txn.channel == CARD_PURCHASE and txn.source in card_set:
            if start <= txn.timestamp < end_excl:
                grouped[txn.source].append(txn)

    return dict(grouped)


def _build_context(request: Request, card: str) -> _CardContext | None:
    owner = request.cards.owner_map.get(card)
    if not owner:
        return None

    pay_from = request.primary_accounts.get(owner)
    if not pay_from:
        return None

    return _CardContext(
        card=card,
        pay_from=pay_from,
        apr=float(request.cards.aprs.get(card, _DEFAULT_CARD_APR)),
        cycle_day=int(request.cards.cycle_days.get(card, _DEFAULT_CYCLE_DAY)),
        autopay_mode=int(request.cards.autopay_modes.get(card, 0)),
    )


def _process_billing_cycle(
    terms: Terms,
    habits: Habits,
    request: Request,
    context: _CardContext,
    state: _CycleState,
    purchases: list[Transaction],
    cycle_start: datetime,
    cycle_end: datetime,
    end_excl: datetime,
    lifecycle_gen: np.random.Generator,
) -> list[Transaction]:
    """Handles the financial business logic for a single billing month."""
    out: list[Transaction] = []
    cycle_events: list[Transaction] = []

    while (
        state.purchase_idx < len(purchases)
        and purchases[state.purchase_idx].timestamp < cycle_end
    ):
        purchase = purchases[state.purchase_idx]
        cycle_events.append(purchase)

        state.credit_idx += 1
        credit_txn = sample_merchant_credit(
            terms,
            habits,
            lifecycle_gen,
            card=context.card,
            purchase=purchase,
            end_excl=end_excl,
            txf=request.txf,
        )
        if credit_txn:
            state.scheduled_credits.append(credit_txn)
            out.append(credit_txn)

        state.purchase_idx += 1

    if state.scheduled_credits:
        keep: list[Transaction] = []
        for credit_txn in state.scheduled_credits:
            if cycle_start <= credit_txn.timestamp < cycle_end:
                cycle_events.append(credit_txn)
            else:
                keep.append(credit_txn)
        state.scheduled_credits = keep

    if cycle_events:
        cycle_events.sort(key=lambda txn: txn.timestamp)

    avg_balance, state.balance = calculate_avg_balance(
        context.card, state.balance, cycle_events, cycle_start, cycle_end
    )

    if not state.in_grace:
        interval_days = calculate_interval_days(cycle_start, cycle_end)
        debt_avg = max(0.0, -float(avg_balance))

        interest_raw = debt_avg * (context.apr / 365.0) * float(interval_days)
        interest = get_billable_interest(interest_raw)

        if interest is not None:
            interest_txn = request.txf.make(
                TransactionDraft(
                    source=context.card,
                    destination=request.issuer_acct,
                    amount=interest,
                    timestamp=cycle_end + timedelta(minutes=15),
                    channel=CC_INTEREST,
                )
            )
            out.append(interest_txn)
            state.balance -= float(interest)

    statement_abs = max(0.0, -state.balance)
    if statement_abs <= 0.01:
        state.in_grace = True
        return out

    min_due = round(calculate_min_due(terms, statement_abs), 2)
    due = cycle_end + timedelta(days=int(terms.grace_days), hours=17)

    if context.autopay_mode == 2:
        pay_amt = float(statement_abs)
        pay_ts = sample_payment_time(habits, request.rng, due, is_autopay=True)
    elif context.autopay_mode == 1:
        pay_amt = float(min_due)
        pay_ts = sample_payment_time(habits, request.rng, due, is_autopay=True)
    else:
        pay_amt = sample_manual_payment(habits, request.rng, statement_abs, min_due)
        pay_ts = sample_payment_time(habits, request.rng, due, is_autopay=False)

    pay_amt = round(max(0.0, float(pay_amt)), 2)

    paid_by_due = (pay_amt >= min_due - 1e-6) and is_on_time(pay_ts, due)
    paid_full_by_due = (pay_amt >= statement_abs - 1e-6) and is_on_time(pay_ts, due)

    if pay_amt > 0.0 and pay_ts < end_excl:
        payment_txn = request.txf.make(
            TransactionDraft(
                source=context.pay_from,
                destination=context.card,
                amount=pay_amt,
                timestamp=pay_ts,
                channel=CC_PAYMENT,
            )
        )
        out.append(payment_txn)
        state.balance += float(pay_amt)

    if not paid_by_due and float(terms.late_fee) > 0.0:
        fee_ts = min(
            end_excl - timedelta(seconds=1),
            resolve_due_date(due) + timedelta(days=1, hours=10),
        )
        fee = round(float(terms.late_fee), 2)
        fee_txn = request.txf.make(
            TransactionDraft(
                source=context.card,
                destination=request.issuer_acct,
                amount=fee,
                timestamp=fee_ts,
                channel=CC_LATE_FEE,
            )
        )
        out.append(fee_txn)
        state.balance -= float(fee)

    state.in_grace = bool(paid_full_by_due)
    return out


def generate(
    terms: Terms,
    habits: Habits,
    request: Request,
) -> list[Transaction]:
    """
    Reads existing card_purchase txns and generates the lifecycle edges:
      - refunds / chargebacks
      - interest & late fees
      - payments
    """
    if not request.cards.ids:
        return []

    start = request.window.start_date
    end_excl = start + timedelta(days=int(request.window.days))
    purchases_by_card = _group_purchases(request, start, end_excl)

    rng_factory = RngFactory(request.base_seed)
    lifecycle_gen = rng_factory.rng("cc_lifecycle").gen

    out: list[Transaction] = []

    for card, purchases in purchases_by_card.items():
        context = _build_context(request, card)
        if not context:
            continue

        purchases.sort(key=lambda txn: txn.timestamp)
        closes = calculate_cycle_closes(start, end_excl, context.cycle_day)
        if not closes:
            continue

        state = _CycleState()
        last_close = start

        for close in closes:
            cycle_txns = _process_billing_cycle(
                terms=terms,
                habits=habits,
                request=request,
                context=context,
                state=state,
                purchases=purchases,
                cycle_start=last_close,
                cycle_end=close,
                end_excl=end_excl,
                lifecycle_gen=lifecycle_gen,
            )
            out.extend(cycle_txns)
            last_close = close

    out.sort(
        key=lambda txn: (
            txn.timestamp,
            txn.source,
            txn.target,
            float(txn.amount),
            txn.channel or "",
        )
    )
    return out
