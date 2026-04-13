from datetime import timedelta
from typing import cast

import numpy as np

from common.config import population as pop_config
from common.persona_names import STUDENT
from common.channels import TUITION
from common.math import as_int
from common.transactions import Transaction
from transfers.factory import TransactionDraft

from .engine import Runtime, Schedule
from .helpers import pick_education_payee


def generate(
    rt: Runtime,
    transfer_cfg: pop_config.Tuition,
    schedule: Schedule,
    gen: np.random.Generator,
) -> list[Transaction]:
    """Generates tuition payments from parents for their student children."""
    if not transfer_cfg.enabled:
        return []

    payee = pick_education_payee(rt.merchants, gen)
    if not payee:
        return []

    txns: list[Transaction] = []

    # Find all students in the family
    students = [
        person_id for person_id, persona in rt.personas.items() if persona == STUDENT
    ]

    for student_id in students:
        if student_id not in rt.family.parents:
            continue

        # Check probability of paying tuition this cycle
        if float(gen.random()) >= float(transfer_cfg.p):
            continue

        # Pick a parent to pay
        parents = rt.family.parents[student_id]
        payer_idx = as_int(cast(int | np.integer, gen.integers(0, len(parents))))
        payer_id = parents[payer_idx]
        payer_acct = rt.primary_accounts.get(payer_id)

        if not payer_acct:
            continue

        # Determine installments and total cost
        num_installments = as_int(
            cast(
                int | np.integer,
                gen.integers(
                    int(transfer_cfg.inst_min),
                    int(transfer_cfg.inst_max) + 1,
                ),
            )
        )

        total_tuition = float(
            gen.lognormal(
                mean=float(transfer_cfg.mu),
                sigma=float(transfer_cfg.sigma),
            )
        )
        total_tuition = round(max(200.0, total_tuition), 2)
        installment_amount = round(total_tuition / max(1, num_installments), 2)

        # Baseline start time (e.g., beginning of the semester/schedule)
        offset_days = as_int(cast(int | np.integer, gen.integers(0, 10)))
        semester_start = schedule.month_starts[0] + timedelta(days=offset_days)

        for i in range(num_installments):
            # Space out installments roughly every 30 days
            ts = semester_start + timedelta(days=30 * i)
            if ts >= schedule.end_excl:
                break

            # Add daily jitter to the specific payment
            jitter_days = as_int(cast(int | np.integer, gen.integers(0, 5)))
            jitter_hours = as_int(cast(int | np.integer, gen.integers(8, 18)))
            jitter_mins = as_int(cast(int | np.integer, gen.integers(0, 60)))

            ts += timedelta(
                days=jitter_days,
                hours=jitter_hours,
                minutes=jitter_mins,
            )

            if ts >= schedule.end_excl:
                break

            # Add slight noise to the installment amount
            noise = float(gen.normal(loc=0.0, scale=0.03))
            final_amount = round(max(10.0, installment_amount * (1.0 + noise)), 2)

            txns.append(
                rt.txf.make(
                    TransactionDraft(
                        source=payer_acct,
                        destination=payee,
                        amount=final_amount,
                        timestamp=ts,
                        channel=TUITION,
                    )
                )
            )

    return txns
