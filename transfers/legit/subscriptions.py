"""
Fixed-amount recurring subscription charges.

This version keeps the fixed recurring structure but adds an
affordability screen using a seeded ledger view and known earlier
candidate transactions.
"""

from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

import numpy as np

from common.channels import SUBSCRIPTION
from common.math import as_int
from common.random import Rng, derive_seed
from common.transactions import Transaction
from common.validate import between, ge
from transfers.balances import Ledger
from transfers.factory import TransactionDraft, TransactionFactory
from transfers.screening import advance_book_through

from .plans import LegitBuildPlan

_PRICE_POOL: tuple[float, ...] = (
    6.99,
    7.99,
    9.99,
    10.99,
    11.99,
    12.99,
    14.99,
    15.49,
    15.99,
    17.99,
    22.99,
    24.99,
    29.99,
    34.99,
    39.99,
    49.99,
    59.99,
    99.99,
)


@dataclass(frozen=True, slots=True)
class SubscriptionConfig:
    min_per_person: int = 4
    max_per_person: int = 8
    debit_p: float = 0.55
    day_jitter: int = 1

    def __post_init__(self) -> None:
        ge("min_per_person", self.min_per_person, 0)
        ge("max_per_person", self.max_per_person, self.min_per_person)
        between("debit_p", self.debit_p, 0.0, 1.0)
        ge("day_jitter", self.day_jitter, 0)


DEFAULT_SUBSCRIPTION_CONFIG = SubscriptionConfig()


@dataclass(frozen=True, slots=True)
class _PersonSub:
    merchant_acct: str
    amount: float
    billing_day: int  # 1-28


def _assign_subscriptions(
    base_seed: int,
    person_id: str,
    cfg: SubscriptionConfig,
    merchant_accts: list[str],
) -> list[_PersonSub]:
    gen = np.random.default_rng(derive_seed(base_seed, "subscriptions", person_id))

    n_total = as_int(
        cast(
            int | np.integer,
            gen.integers(cfg.min_per_person, cfg.max_per_person + 1),
        )
    )

    n_debit = sum(1 for _ in range(n_total) if float(gen.random()) < cfg.debit_p)
    if n_debit == 0:
        return []

    n_pick = min(n_debit, len(_PRICE_POOL))
    price_indices = gen.choice(len(_PRICE_POOL), size=n_pick, replace=False)
    prices = [_PRICE_POOL[int(price_indices.item(i))] for i in range(n_pick)]

    merchant_indices = gen.choice(len(merchant_accts), size=n_pick)

    subs: list[_PersonSub] = []
    for i in range(n_pick):
        billing_day = as_int(cast(int | np.integer, gen.integers(1, 29)))
        subs.append(
            _PersonSub(
                merchant_acct=merchant_accts[int(merchant_indices.item(i))],
                amount=prices[i],
                billing_day=billing_day,
            )
        )
    return subs


def generate(
    rng: Rng,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
    cfg: SubscriptionConfig = DEFAULT_SUBSCRIPTION_CONFIG,
    *,
    book: Ledger | None = None,
    base_txns: list[Transaction] | None = None,
) -> list[Transaction]:
    if not plan.paydays:
        return []

    merchant_accts = plan.counterparties.biller_accounts
    if not merchant_accts:
        return []

    end_excl = plan.start_date + timedelta(days=plan.days)
    txns: list[Transaction] = []

    seeded = sorted(base_txns or [], key=lambda t: t.timestamp)
    seed_idx = 0

    subscriptions_by_person: list[tuple[str, str, list[_PersonSub]]] = []
    for person_id in plan.persons:
        deposit_acct = plan.primary_acct_for_person.get(person_id)
        if not deposit_acct:
            continue
        if deposit_acct in plan.counterparties.hub_set:
            continue

        subs = _assign_subscriptions(
            plan.seed,
            person_id,
            cfg,
            merchant_accts,
        )
        if subs:
            subscriptions_by_person.append((person_id, deposit_acct, subs))

    for month_start in plan.paydays:
        month_candidates: list[tuple[datetime, str, str, float]] = []

        for _, deposit_acct, subs in subscriptions_by_person:
            for sub in subs:
                day = min(sub.billing_day, 28)
                jitter = rng.int(-cfg.day_jitter, cfg.day_jitter + 1)
                day = max(1, min(28, day + jitter))

                ts = month_start.replace(day=1) + timedelta(
                    days=day - 1,
                    hours=rng.int(0, 6),
                    minutes=rng.int(0, 60),
                )

                if ts < plan.start_date or ts >= end_excl:
                    continue

                month_candidates.append(
                    (ts, deposit_acct, sub.merchant_acct, sub.amount)
                )

        for ts, src, dst, amount in sorted(month_candidates, key=lambda x: x[0]):
            seed_idx = advance_book_through(
                book,
                seeded,
                seed_idx,
                ts,
                inclusive=True,
            )
            if book is not None:
                decision = book.try_transfer_with_reason(
                    src,
                    dst,
                    amount,
                    channel=SUBSCRIPTION,
                    timestamp=ts,
                )
                if not decision.accepted:
                    continue

            txns.append(
                txf.make(
                    TransactionDraft(
                        source=src,
                        destination=dst,
                        amount=amount,
                        timestamp=ts,
                        channel=SUBSCRIPTION,
                    )
                )
            )

    return txns
