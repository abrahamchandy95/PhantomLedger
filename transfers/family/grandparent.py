"""
Grandparent-to-grandchild gifts.

38-45% of households aged 50+ transfer money to younger generations
(EBRI 2015). Average transfer from 50-64 year olds: ~$8,350 over
two years. These are modeled as monthly gifts from retired personas
who have children that themselves have children (implicit grandchildren).

We identify grandparent→grandchild pairs by traversing:
  retired person → their children (via family.children) →
  those children's children (via family.children again)
"""

from datetime import timedelta
from typing import cast

import numpy as np

from common.config import population as pop_config
from common.channels import GRANDPARENT_GIFT
from common.math import as_int, lognormal_by_median
from common.transactions import Transaction
from transfers.factory import TransactionDraft

from .engine import Runtime, Schedule


def _find_grandparent_pairs(
    rt: Runtime,
) -> list[tuple[str, str]]:
    """Finds (grandparent, grandchild) pairs via two-hop traversal."""
    children_map = rt.family.children
    pairs: list[tuple[str, str]] = []

    for gp_id, persona in rt.personas.items():
        if persona != "retired":
            continue

        middle_gen = children_map.get(gp_id, [])
        for parent_id in middle_gen:
            grandchildren = children_map.get(parent_id, [])
            for gc_id in grandchildren:
                pairs.append((gp_id, gc_id))

    return pairs


def generate(
    rt: Runtime,
    gift_cfg: pop_config.GrandparentGifts,
    schedule: Schedule,
    gen: np.random.Generator,
) -> list[Transaction]:
    """Generates monthly gifts from grandparents to grandchildren."""
    if not gift_cfg.enabled:
        return []

    pairs = _find_grandparent_pairs(rt)
    if not pairs:
        return []

    txns: list[Transaction] = []

    for gp_id, gc_id in pairs:
        gp_acct = rt.primary_accounts.get(gp_id)
        gc_acct = rt.primary_accounts.get(gc_id)

        if not gp_acct or not gc_acct or gp_acct == gc_acct:
            continue

        for month_start in schedule.month_starts:
            if float(gen.random()) >= float(gift_cfg.p):
                continue

            amt = float(
                lognormal_by_median(
                    gen,
                    median=float(gift_cfg.median),
                    sigma=float(gift_cfg.sigma),
                )
            )
            amt = round(max(10.0, amt), 2)

            offset_days = as_int(cast(int | np.integer, gen.integers(0, 20)))
            offset_hours = as_int(cast(int | np.integer, gen.integers(8, 20)))
            offset_mins = as_int(cast(int | np.integer, gen.integers(0, 60)))

            ts = month_start + timedelta(
                days=offset_days,
                hours=offset_hours,
                minutes=offset_mins,
            )
            if ts >= schedule.end_excl:
                break

            txns.append(
                rt.txf.make(
                    TransactionDraft(
                        source=gp_acct,
                        destination=gc_acct,
                        amount=amt,
                        timestamp=ts,
                        channel=GRANDPARENT_GIFT,
                    )
                )
            )

    return txns
