"""
Parent-to-adult-child financial gifts.

Downward transfers (parent → adult child) are far more common than
upward ones. HRS longitudinal data: 35% of parents aged 51+ gave a
financial transfer to children over a two-year period, while only
4.9% received one from children. Over a 20-year period, 48% of adult
children receive at least one transfer (ScienceDirect 2023).

As of 2025, 50% of parents financially support adult children, with
average support exceeding $1,300/month among those who give (Savings.com).

This module generates gifts from non-retired, non-student parents to
their adult (non-student) children. Student children are already
covered by the allowance and tuition generators.
"""

from datetime import timedelta
from typing import cast

import numpy as np

from common.channels import PARENT_GIFT
from common.family_accounts import resolve_family_acct
from common.math import as_int
from common.persona_names import SALARIED, FREELANCER, SMALLBIZ, HNW
from common.transactions import Transaction
from transfers.factory import TransactionDraft

from .engine import GenerateRequest, Schedule
from .helpers import pareto_amount, support_capacity_weight


def _is_working_adult(persona: str) -> bool:
    """Parents who can give: salaried, freelancer, smallbiz, hnw."""
    return persona in {SALARIED, FREELANCER, SMALLBIZ, HNW}


def _is_adult_child(persona: str) -> bool:
    """Recipients: non-student adults. Students get allowances/tuition instead."""
    return persona != "student"


def generate(
    request: GenerateRequest,
    schedule: Schedule,
    gen: np.random.Generator,
) -> list[Transaction]:
    """Generates financial gifts from working parents to adult children."""
    params = request.params
    if not params.parent_gift_enabled:
        return []

    txns: list[Transaction] = []

    for child_id, parent_ids in request.family.parents.items():
        child_persona = request.personas.get(child_id, SALARIED)
        if not _is_adult_child(child_persona):
            continue

        child_acct = request.primary_accounts.get(child_id)
        if not child_acct:
            continue

        giving_parents = [
            pid
            for pid in parent_ids
            if _is_working_adult(request.personas.get(pid, SALARIED))
        ]
        if not giving_parents:
            continue

        for month_start in schedule.month_starts:
            if float(gen.random()) >= float(params.parent_gift_p):
                continue

            payer_idx = as_int(
                cast(
                    int | np.integer,
                    gen.integers(0, len(giving_parents)),
                )
            )
            payer_id = giving_parents[payer_idx]
            payer_acct = resolve_family_acct(
                payer_id,
                request.primary_accounts,
                float(request.params.external_family_p),
            )
            if not payer_acct or payer_acct == child_acct:
                continue

            offset_days = as_int(cast(int | np.integer, gen.integers(0, 20)))
            offset_hours = as_int(cast(int | np.integer, gen.integers(8, 21)))
            offset_mins = as_int(cast(int | np.integer, gen.integers(0, 60)))

            ts = month_start + timedelta(
                days=offset_days,
                hours=offset_hours,
                minutes=offset_mins,
            )
            if ts >= schedule.end_excl:
                break

            base_amount = pareto_amount(
                request.rng,
                xm=float(params.parent_gift_pareto_xm),
                alpha=float(params.parent_gift_pareto_alpha),
            )
            multiplier = support_capacity_weight(payer_id, request.persona_objects)
            final_amount = round(max(10.0, base_amount * multiplier), 2)

            txns.append(
                request.txf.make(
                    TransactionDraft(
                        source=payer_acct,
                        destination=child_acct,
                        amount=final_amount,
                        timestamp=ts,
                        channel=PARENT_GIFT,
                    )
                )
            )

    return txns
