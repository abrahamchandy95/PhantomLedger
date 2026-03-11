from datetime import timedelta
from typing import cast

import numpy as np

from common.math import as_int
from common.transactions import Transaction
from transfers.txns import TxnSpec

from .helpers import pareto_amount
from .models import FamilyTransferRequest, FamilyTransferSchedule


def generate_allowance_txns(
    request: FamilyTransferRequest,
    schedule: FamilyTransferSchedule,
    gen: np.random.Generator,
) -> list[Transaction]:
    fcfg = request.family_cfg
    if not fcfg.allowance_enabled:
        return []

    txns: list[Transaction] = []

    for child, parents in request.family.parents_of.items():
        if request.persona_for_person.get(child) != "student":
            continue

        child_acct = request.primary_acct_for_person.get(child)
        if child_acct is None:
            continue

        weekly = float(gen.random()) < float(fcfg.allowance_weekly_p)
        step_days = 7 if weekly else 14

        t = schedule.start_date + timedelta(
            days=as_int(cast(int | np.integer, gen.integers(0, 14))),
            hours=as_int(cast(int | np.integer, gen.integers(7, 21))),
            minutes=as_int(cast(int | np.integer, gen.integers(0, 60))),
        )

        while t < schedule.end_excl:
            payer_idx = as_int(cast(int | np.integer, gen.integers(0, len(parents))))
            payer_person = parents[payer_idx]
            payer_acct = request.primary_acct_for_person.get(payer_person)

            if payer_acct is not None and payer_acct != child_acct:
                amt = pareto_amount(
                    request.rng,
                    xm=float(fcfg.allowance_pareto_xm),
                    alpha=float(fcfg.allowance_pareto_alpha),
                )
                txns.append(
                    request.txf.make(
                        TxnSpec(
                            src=payer_acct,
                            dst=child_acct,
                            amt=amt,
                            ts=t,
                            channel="allowance",
                        )
                    )
                )

            jitter = as_int(cast(int | np.integer, gen.integers(-1, 2)))
            t = t + timedelta(days=step_days + jitter)

    return txns
