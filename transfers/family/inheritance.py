"""
Inheritance events.

26% of Americans plan to leave an inheritance (NW Mutual 2024).
Modeled as a rare, one-time large lump-sum transfer from a retired
persona to their children. In a 180-day window, ~0.15% of retirees
trigger this event.

The transfer is split equally among children if multiple exist.
Source is the retiree's account; this doesn't model death explicitly,
just the observable bank-side event of a large outflow to heirs.
"""

from datetime import timedelta
from typing import cast

import numpy as np

from common.channels import INHERITANCE
from common.family_accounts import resolve_family_acct
from common.math import as_int, lognormal_by_median
from common.transactions import Transaction
from transfers.factory import TransactionDraft

from .engine import GenerateRequest, Schedule


def generate(
    request: GenerateRequest,
    schedule: Schedule,
    gen: np.random.Generator,
) -> list[Transaction]:
    """Generates rare inheritance lump-sum transfers."""
    params = request.params
    if not params.inheritance_enabled:
        return []

    txns: list[Transaction] = []
    external_p = float(params.external_family_p)

    window_days = max(1, (schedule.end_excl - schedule.start_date).days)

    retirees = [
        pid for pid, persona in request.personas.items() if persona == "retired"
    ]

    for retiree_id in retirees:
        if float(gen.random()) >= float(params.inheritance_event_p):
            continue

        retiree_acct = request.primary_accounts.get(retiree_id)
        if not retiree_acct:
            continue

        # Find heirs: direct children first, then supporting_children
        heirs = list(request.family.children.get(retiree_id, []))
        if not heirs:
            heirs = list(request.family.supporting_children.get(retiree_id, []))
        if not heirs:
            continue

        total = float(
            lognormal_by_median(
                gen,
                median=float(params.inheritance_median),
                sigma=float(params.inheritance_sigma),
            )
        )
        total = max(1000.0, total)
        per_heir = round(total / len(heirs), 2)

        offset_days = as_int(cast(int | np.integer, gen.integers(0, window_days)))
        ts = schedule.start_date + timedelta(
            days=offset_days,
            hours=as_int(cast(int | np.integer, gen.integers(10, 16))),
            minutes=as_int(cast(int | np.integer, gen.integers(0, 60))),
        )
        if ts >= schedule.end_excl:
            continue

        for heir_id in heirs:
            heir_acct = resolve_family_acct(
                heir_id,
                request.primary_accounts,
                external_p,
            )
            if not heir_acct or heir_acct == retiree_acct:
                continue

            txns.append(
                request.txf.make(
                    TransactionDraft(
                        source=retiree_acct,
                        destination=heir_acct,
                        amount=per_heir,
                        timestamp=ts,
                        channel=INHERITANCE,
                    )
                )
            )

    return txns
