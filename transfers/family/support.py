from datetime import timedelta
from typing import cast

import numpy as np

from common.math import as_int
from common.types import Txn
from transfers.txns import TxnSpec

from .helpers import pareto_amount, support_capacity_weight, weighted_pick_person
from .models import FamilyTransferRequest, FamilyTransferSchedule


def generate_support_txns(
    request: FamilyTransferRequest,
    schedule: FamilyTransferSchedule,
    gen: np.random.Generator,
) -> list[Txn]:
    fcfg = request.family_cfg
    if not fcfg.retiree_support_enabled:
        return []

    txns: list[Txn] = []

    retirees = [
        person_id
        for person_id, persona in request.persona_for_person.items()
        if persona == "retired"
    ]

    for retiree_id in retirees:
        adult_kids = request.family.adult_children_of.get(retiree_id)
        if not adult_kids:
            continue

        retiree_acct = request.primary_acct_for_person.get(retiree_id)
        if retiree_acct is None:
            continue

        for month_start in schedule.month_starts:
            if float(gen.random()) >= float(fcfg.retiree_support_p):
                continue

            payer_person = weighted_pick_person(
                adult_kids,
                request.persona_for_person,
                gen,
            )
            payer_acct = request.primary_acct_for_person.get(payer_person)
            if payer_acct is None or payer_acct == retiree_acct:
                continue

            ts = month_start + timedelta(
                days=as_int(cast(int | np.integer, gen.integers(0, 6))),
                hours=as_int(cast(int | np.integer, gen.integers(7, 21))),
                minutes=as_int(cast(int | np.integer, gen.integers(0, 60))),
            )
            if ts >= schedule.end_excl:
                break

            base_amt = pareto_amount(
                request.rng,
                xm=float(fcfg.retiree_support_pareto_xm),
                alpha=float(fcfg.retiree_support_pareto_alpha),
            )
            mult = support_capacity_weight(
                request.persona_for_person.get(payer_person, "salaried")
            )
            amt = round(max(5.0, base_amt * mult), 2)

            txns.append(
                request.txf.make(
                    TxnSpec(
                        src=payer_acct,
                        dst=retiree_acct,
                        amt=amt,
                        ts=ts,
                        channel="family_support",
                    )
                )
            )

    return txns
