from datetime import timedelta
from typing import cast

import numpy as np

from common.channels import FAMILY_SUPPORT
from common.family_accounts import resolve_family_acct
from common.math import as_int
from common.persona_names import RETIRED
from common.transactions import Transaction
from transfers.factory import TransactionDraft

from .engine import GenerateRequest, Schedule
from .helpers import (
    weighted_pick_person,
    support_capacity_weight,
    pareto_amount,
)


def generate(
    request: GenerateRequest,
    schedule: Schedule,
    gen: np.random.Generator,
) -> list[Transaction]:
    """Generates financial support transactions from adult children to retired parents."""
    params = request.params
    if not params.retiree_support_enabled:
        return []

    txns: list[Transaction] = []

    retirees = [
        person_id
        for person_id, persona in request.personas.items()
        if persona == RETIRED
    ]

    for retiree_id in retirees:
        adult_kids = request.family.supporting_children.get(retiree_id)
        if not adult_kids:
            continue

        retiree_acct = request.primary_accounts.get(retiree_id)
        if not retiree_acct:
            continue

        for month_start in schedule.month_starts:
            if float(gen.random()) >= float(params.retiree_support_p):
                continue

            payer_id = weighted_pick_person(
                adult_kids,
                request.persona_objects,
                gen,
            )
            payer_acct = resolve_family_acct(
                payer_id,
                request.primary_accounts,
                float(request.params.external_family_p),
            )
            if not payer_acct or payer_acct == retiree_acct:
                continue

            offset_days = as_int(cast(int | np.integer, gen.integers(0, 6)))
            offset_hours = as_int(cast(int | np.integer, gen.integers(7, 21)))
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
                xm=float(params.retiree_support_pareto_xm),
                alpha=float(params.retiree_support_pareto_alpha),
            )
            multiplier = support_capacity_weight(payer_id, request.persona_objects)
            final_amount = round(max(5.0, base_amount * multiplier), 2)

            txns.append(
                request.txf.make(
                    TransactionDraft(
                        source=payer_acct,
                        destination=retiree_acct,
                        amount=final_amount,
                        timestamp=ts,
                        channel=FAMILY_SUPPORT,
                    )
                )
            )

    return txns
