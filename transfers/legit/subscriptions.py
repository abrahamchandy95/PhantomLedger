"""
Fixed-amount recurring subscription charges.

Real bank data has a distinct pattern of exact-amount monthly charges
(Netflix $15.49, gym $29.99, insurance $187.00) hitting the same
merchant on roughly the same calendar day each month.

Statistics (C+R Research 2024, Just Cancel 2026):
- Average American has ~12 subscriptions totaling $219/month
- Many route through credit cards; 4-8 hit checking accounts directly
- Common price points cluster around specific values

This generator assigns each person a fixed set of subscription merchants
with exact amounts that repeat monthly. The amounts never vary — this
is the key distinguishing signal vs randomized merchant payments.
"""

from dataclasses import dataclass
from datetime import timedelta
from typing import cast

import numpy as np

from common.channels import SUBSCRIPTION
from common.math import as_int
from common.random import Rng, derive_seed
from common.transactions import Transaction
from common.validate import between, ge
from transfers.factory import TransactionDraft, TransactionFactory

from .plans import LegitBuildPlan


# Realistic subscription price points drawn from actual US market prices.
# Each person draws 4-8 of these without replacement.
_PRICE_POOL: tuple[float, ...] = (
    6.99,  # basic streaming tier / cloud storage
    7.99,  # ad-supported streaming
    9.99,  # music streaming / news
    10.99,  # music streaming
    11.99,  # spotify premium
    12.99,  # cloud storage / software
    14.99,  # standard streaming
    15.49,  # netflix standard
    15.99,  # streaming / gym
    17.99,  # premium streaming
    22.99,  # premium streaming
    24.99,  # gym membership
    29.99,  # gym / software
    34.99,  # fitness class
    39.99,  # premium gym / software
    49.99,  # premium service
    59.99,  # premium bundle
    99.99,  # annual-billed monthly (insurance add-on, etc)
)


@dataclass(frozen=True, slots=True)
class SubscriptionConfig:
    """Controls the subscription generation."""

    min_per_person: int = 4
    max_per_person: int = 8

    # Probability that a subscription hits the debit account
    # vs credit card (handled elsewhere). This filters how many
    # of a person's subscriptions show up as bank debits.
    debit_p: float = 0.55

    # Day-of-month jitter: subscriptions renew on a fixed day
    # but can vary +/- this many days across months
    day_jitter: int = 1

    def __post_init__(self) -> None:
        ge("min_per_person", self.min_per_person, 0)
        ge("max_per_person", self.max_per_person, self.min_per_person)
        between("debit_p", self.debit_p, 0.0, 1.0)
        ge("day_jitter", self.day_jitter, 0)


DEFAULT_SUBSCRIPTION_CONFIG = SubscriptionConfig()


@dataclass(frozen=True, slots=True)
class _PersonSub:
    """A single subscription slot for one person."""

    merchant_acct: str
    amount: float
    billing_day: int  # 1-28


def _assign_subscriptions(
    base_seed: int,
    person_id: str,
    cfg: SubscriptionConfig,
    merchant_accts: list[str],
) -> list[_PersonSub]:
    """Deterministically assign subscriptions to a person."""
    gen = np.random.default_rng(derive_seed(base_seed, "subscriptions", person_id))

    n_total = as_int(
        cast(
            int | np.integer,
            gen.integers(cfg.min_per_person, cfg.max_per_person + 1),
        )
    )

    # Filter to debit-paid subscriptions
    n_debit = sum(1 for _ in range(n_total) if float(gen.random()) < cfg.debit_p)
    if n_debit == 0:
        return []

    # Pick prices without replacement from the pool
    n_pick = min(n_debit, len(_PRICE_POOL))
    price_indices = gen.choice(len(_PRICE_POOL), size=n_pick, replace=False)
    prices = [_PRICE_POOL[int(price_indices.item(i))] for i in range(n_pick)]

    # Pick merchants (with replacement — multiple subs can go to same merchant)
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
) -> list[Transaction]:
    """Generates fixed-amount recurring subscription charges."""
    if not plan.paydays:
        return []

    merchant_accts = plan.counterparties.biller_accounts
    if not merchant_accts:
        return []

    txns: list[Transaction] = []

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

        for sub in subs:
            for month_start in plan.paydays:
                # Clamp billing day to the month
                day = min(sub.billing_day, 28)
                jitter = rng.int(-cfg.day_jitter, cfg.day_jitter + 1)
                day = max(1, min(28, day + jitter))

                ts = month_start.replace(day=1) + timedelta(
                    days=day - 1,
                    hours=rng.int(0, 6),  # subscriptions charge early morning
                    minutes=rng.int(0, 60),
                )

                if ts < plan.start_date:
                    continue
                end_excl = plan.start_date + timedelta(days=plan.days)
                if ts >= end_excl:
                    continue

                txns.append(
                    txf.make(
                        TransactionDraft(
                            source=deposit_acct,
                            destination=sub.merchant_acct,
                            amount=sub.amount,
                            timestamp=ts,
                            channel=SUBSCRIPTION,
                        )
                    )
                )

    return txns
