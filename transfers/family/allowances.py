from datetime import timedelta
from typing import cast

import numpy as np

from common.channels import ALLOWANCE
from common.family_accounts import resolve_family_acct
from common.math import as_int
from common.persona_names import STUDENT
from common.transactions import Transaction
from transfers.factory import TransactionDraft

from .engine import GenerateRequest, Schedule
from .helpers import pareto_amount


def generate(
    request: GenerateRequest,
    schedule: Schedule,
    gen: np.random.Generator,
) -> list[Transaction]:
    """Generates recurring allowance transactions from parents to student children."""
    params = request.params
    if not params.allowance_enabled:
        return []

    txns: list[Transaction] = []

    for child_id, parents in request.family.parents.items():
        if request.personas.get(child_id) != STUDENT:
            continue

        child_acct = request.primary_accounts.get(child_id)
        if not child_acct:
            continue

        is_weekly = gen.random() < float(params.allowance_weekly_p)
        interval_days = 7 if is_weekly else 14

        # Calculate the initial offset for the first allowance payment
        offset_days = as_int(cast(int | np.integer, gen.integers(0, 14)))
        offset_hours = as_int(cast(int | np.integer, gen.integers(7, 21)))
        offset_mins = as_int(cast(int | np.integer, gen.integers(0, 60)))

        ts = schedule.start_date + timedelta(
            days=offset_days,
            hours=offset_hours,
            minutes=offset_mins,
        )

        # Loop through the schedule window and process payouts
        while ts < schedule.end_excl:
            payer_idx = as_int(cast(int | np.integer, gen.integers(0, len(parents))))
            payer_id = parents[payer_idx]
            payer_acct = resolve_family_acct(
                payer_id,
                request.primary_accounts,
                float(request.params.external_family_p),
            )
            if payer_acct and payer_acct != child_acct:
                amt = pareto_amount(
                    request.rng,
                    xm=float(params.allowance_pareto_xm),
                    alpha=float(params.allowance_pareto_alpha),
                )

                txns.append(
                    request.txf.make(
                        TransactionDraft(
                            source=payer_acct,
                            destination=child_acct,
                            amount=amt,
                            timestamp=ts,
                            channel=ALLOWANCE,
                        )
                    )
                )

            # Advance to the next payment with slight schedule jitter
            jitter_days = as_int(cast(int | np.integer, gen.integers(-1, 2)))
            ts += timedelta(days=interval_days + jitter_days)

    return txns
