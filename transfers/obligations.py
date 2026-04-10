"""
Obligation event emitter.

Converts ObligationEvents from the product portfolio into
concrete Transactions. This is the bridge between the pure
product layer (no infra, no fraud flags) and the transaction
pipeline (with device/IP routing and balance constraints).
"""

from datetime import datetime, timedelta

from common.random import Rng
from common.transactions import Transaction
from entities.products.event import Direction
from entities.products.portfolio import PortfolioRegistry
from transfers.factory import TransactionDraft, TransactionFactory


def _add_jitter(rng: Rng, ts: datetime) -> datetime:
    """
    Add small timing jitter to scheduled events.

    Real payments don't land at exactly midnight on the due date.
    ACH processing, autopay timing, and manual payment habits
    introduce 0-3 days and variable hours of offset.
    """
    return ts + timedelta(
        days=rng.int(0, 3),
        hours=rng.int(0, 12),
        minutes=rng.int(0, 60),
    )


def emit(
    registry: PortfolioRegistry,
    *,
    start: datetime,
    end_excl: datetime,
    primary_accounts: dict[str, str],
    rng: Rng,
    txf: TransactionFactory,
) -> list[Transaction]:
    """
    Convert all scheduled obligation events into Transactions.

    Skips events where the person has no primary account
    (external-only or unresolvable).
    """
    txns: list[Transaction] = []

    for event in registry.all_events(start, end_excl):
        person_acct = primary_accounts.get(event.person_id)
        if not person_acct:
            continue

        ts = _add_jitter(rng, event.timestamp)
        if ts >= end_excl:
            continue

        if event.direction == Direction.OUTFLOW:
            src = person_acct
            dst = event.counterparty_acct
        else:
            src = event.counterparty_acct
            dst = person_acct

        txns.append(
            txf.make(
                TransactionDraft(
                    source=src,
                    destination=dst,
                    amount=event.amount,
                    timestamp=ts,
                    channel=event.channel,
                )
            )
        )

    return txns
