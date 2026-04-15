from collections.abc import Sequence
from dataclasses import dataclass
from datetime import datetime

import numpy as np

from common.channels import (
    CARD_SETTLEMENT,
    CLIENT_ACH_CREDIT,
    INVESTMENT_INFLOW,
    OWNER_DRAW,
    PLATFORM_PAYOUT,
)
from common.math import lognormal_by_median
from common.transactions import Transaction
from transfers.factory import TransactionDraft, TransactionFactory

from .clock import business_day_ts
from .draw import payment_count, pick_one
from .profiles import CounterpartyRevenueProfile, RevenueFlowProfile


@dataclass(slots=True)
class Cycle:
    """The temporal and execution state for one month of revenue generation."""

    rng: np.random.Generator
    month_start: datetime
    window_start: datetime
    end_excl: datetime
    txf: TransactionFactory
    txns: list[Transaction]


def _append_flow(
    cycle: Cycle,
    *,
    src: str,
    dst: str,
    amount: float,
    timestamp: datetime,
    channel: str,
) -> None:
    if amount <= 0.0:
        return
    if timestamp < cycle.window_start or timestamp >= cycle.end_excl:
        return

    cycle.txns.append(
        cycle.txf.make(
            TransactionDraft(
                source=src,
                destination=dst,
                amount=round(amount, 2),
                timestamp=timestamp,
                channel=channel,
            )
        )
    )


def sort_txns(txns: list[Transaction]) -> None:
    txns.sort(
        key=lambda txn: (
            txn.timestamp,
            txn.source,
            txn.target,
            float(txn.amount),
            txn.channel or "",
        )
    )


def draft_clients(
    cycle: Cycle,
    profile: CounterpartyRevenueProfile,
    dst: str,
    clients: Sequence[str],
) -> None:
    if not clients:
        return

    n = payment_count(cycle.rng, low=profile.payments_min, high=profile.payments_max)
    for _ in range(n):
        src = pick_one(cycle.rng, clients)
        if src is None:
            continue

        amount = float(
            lognormal_by_median(
                cycle.rng,
                median=profile.median,
                sigma=profile.sigma,
            )
        )
        _append_flow(
            cycle,
            src=src,
            dst=dst,
            amount=max(75.0, amount),
            timestamp=business_day_ts(
                cycle.month_start,
                cycle.rng,
                earliest_hour=8,
                latest_hour=17,
                start_day=0,
                end_day_exclusive=28,
            ),
            channel=CLIENT_ACH_CREDIT,
        )


def draft_platforms(
    cycle: Cycle,
    profile: CounterpartyRevenueProfile,
    dst: str,
    platforms: Sequence[str],
) -> None:
    if not platforms:
        return

    n = payment_count(cycle.rng, low=profile.payments_min, high=profile.payments_max)
    for _ in range(n):
        src = pick_one(cycle.rng, platforms)
        if src is None:
            continue

        amount = float(
            lognormal_by_median(
                cycle.rng,
                median=profile.median,
                sigma=profile.sigma,
            )
        )
        _append_flow(
            cycle,
            src=src,
            dst=dst,
            amount=max(25.0, amount),
            timestamp=business_day_ts(
                cycle.month_start,
                cycle.rng,
                earliest_hour=6,
                latest_hour=11,
                start_day=0,
                end_day_exclusive=28,
            ),
            channel=PLATFORM_PAYOUT,
        )


def draft_settlements(
    cycle: Cycle,
    profile: RevenueFlowProfile,
    dst: str,
    processor: str | None,
) -> None:
    if processor is None:
        return

    n = payment_count(cycle.rng, low=profile.payments_min, high=profile.payments_max)
    for _ in range(n):
        amount = float(
            lognormal_by_median(
                cycle.rng,
                median=profile.median,
                sigma=profile.sigma,
            )
        )
        _append_flow(
            cycle,
            src=processor,
            dst=dst,
            amount=max(20.0, amount),
            timestamp=business_day_ts(
                cycle.month_start,
                cycle.rng,
                earliest_hour=5,
                latest_hour=9,
                start_day=0,
                end_day_exclusive=28,
            ),
            channel=CARD_SETTLEMENT,
        )


def draft_draws(
    cycle: Cycle,
    profile: RevenueFlowProfile,
    dst: str,
    src: str | None,
) -> None:
    if src is None:
        return

    n = payment_count(cycle.rng, low=profile.payments_min, high=profile.payments_max)
    for _ in range(n):
        amount = float(
            lognormal_by_median(
                cycle.rng,
                median=profile.median,
                sigma=profile.sigma,
            )
        )
        _append_flow(
            cycle,
            src=src,
            dst=dst,
            amount=max(100.0, amount),
            timestamp=business_day_ts(
                cycle.month_start,
                cycle.rng,
                earliest_hour=10,
                latest_hour=17,
                start_day=8,
                end_day_exclusive=28,
            ),
            channel=OWNER_DRAW,
        )


def draft_investments(
    cycle: Cycle,
    profile: RevenueFlowProfile,
    dst: str,
    src: str | None,
) -> None:
    if src is None:
        return

    n = payment_count(cycle.rng, low=profile.payments_min, high=profile.payments_max)
    for _ in range(n):
        amount = float(
            lognormal_by_median(
                cycle.rng,
                median=profile.median,
                sigma=profile.sigma,
            )
        )
        _append_flow(
            cycle,
            src=src,
            dst=dst,
            amount=max(250.0, amount),
            timestamp=business_day_ts(
                cycle.month_start,
                cycle.rng,
                earliest_hour=7,
                latest_hour=15,
                start_day=0,
                end_day_exclusive=28,
            ),
            channel=INVESTMENT_INFLOW,
        )
