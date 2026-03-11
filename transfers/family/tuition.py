from datetime import timedelta
from typing import cast

import numpy as np

from common.math import as_int
from common.transactions import Transaction
from transfers.txns import TxnSpec

from .helpers import pick_education_payee
from .models import FamilyTransferRequest, FamilyTransferSchedule


def generate_tuition_txns(
    request: FamilyTransferRequest,
    schedule: FamilyTransferSchedule,
    gen: np.random.Generator,
) -> list[Transaction]:
    fcfg = request.family_cfg
    if not fcfg.tuition_enabled:
        return []

    payee = pick_education_payee(request.merchants, gen)
    if payee is None:
        return []

    txns: list[Transaction] = []

    students = [
        person_id
        for person_id, persona in request.persona_for_person.items()
        if persona == "student"
    ]

    for student_id in students:
        if student_id not in request.family.parents_of:
            continue
        if float(gen.random()) >= float(fcfg.tuition_students_p):
            continue

        parents = request.family.parents_of[student_id]
        payer_idx = as_int(cast(int | np.integer, gen.integers(0, len(parents))))
        payer_person = parents[payer_idx]
        payer_acct = request.primary_acct_for_person.get(payer_person)
        if payer_acct is None:
            continue

        n_inst = as_int(
            cast(
                int | np.integer,
                gen.integers(
                    int(fcfg.tuition_installments_min),
                    int(fcfg.tuition_installments_max) + 1,
                ),
            )
        )

        total = float(
            gen.lognormal(
                mean=float(fcfg.tuition_total_logn_mu),
                sigma=float(fcfg.tuition_total_logn_sigma),
            )
        )
        total = round(max(200.0, total), 2)
        installment_amount = round(total / max(1, n_inst), 2)

        semester_start = schedule.month_starts[0] + timedelta(
            days=as_int(cast(int | np.integer, gen.integers(0, 10)))
        )

        for installment_idx in range(n_inst):
            ts = semester_start + timedelta(days=30 * installment_idx)
            if ts >= schedule.end_excl:
                break

            ts = ts + timedelta(
                days=as_int(cast(int | np.integer, gen.integers(0, 5))),
                hours=as_int(cast(int | np.integer, gen.integers(8, 18))),
                minutes=as_int(cast(int | np.integer, gen.integers(0, 60))),
            )
            if ts >= schedule.end_excl:
                break

            noise = float(gen.normal(loc=0.0, scale=0.03))
            amt = round(max(10.0, installment_amount * (1.0 + noise)), 2)

            txns.append(
                request.txf.make(
                    TxnSpec(
                        src=payer_acct,
                        dst=payee,
                        amt=amt,
                        ts=ts,
                        channel="tuition",
                    )
                )
            )

    return txns
