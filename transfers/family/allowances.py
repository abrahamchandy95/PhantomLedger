from datetime import timedelta
from typing import cast

import numpy as np

from common.config import population as pop_config
from common.channels import ALLOWANCE
from common.family_accounts import resolve_family_acct
from common.math import as_int
from common.persona_names import STUDENT
from common.transactions import Transaction
from transfers.factory import TransactionDraft

from .engine import Runtime, Schedule
from .helpers import pareto_amount


def generate(
    rt: Runtime,
    allowance_cfg: pop_config.Allowances,
    routing_cfg: pop_config.Routing,
    schedule: Schedule,
    gen: np.random.Generator,
) -> list[Transaction]:
    """Generates recurring allowance transactions from parents to student children."""
    if not allowance_cfg.enabled:
        return []

    txns: list[Transaction] = []

    for child_id, parents in rt.family.parents.items():
        if rt.personas.get(child_id) != STUDENT:
            continue

        child_acct = rt.primary_accounts.get(child_id)
        if not child_acct:
            continue

        is_weekly = gen.random() < float(allowance_cfg.weekly_p)
        interval_days = 7 if is_weekly else 14

        offset_days = as_int(cast(int | np.integer, gen.integers(0, 14)))
        offset_hours = as_int(cast(int | np.integer, gen.integers(7, 21)))
        offset_mins = as_int(cast(int | np.integer, gen.integers(0, 60)))

        ts = schedule.start_date + timedelta(
            days=offset_days,
            hours=offset_hours,
            minutes=offset_mins,
        )

        while ts < schedule.end_excl:
            payer_idx = as_int(cast(int | np.integer, gen.integers(0, len(parents))))
            payer_id = parents[payer_idx]
            payer_acct = resolve_family_acct(
                payer_id,
                rt.primary_accounts,
                float(routing_cfg.external_p),
            )
            if payer_acct and payer_acct != child_acct:
                amt = pareto_amount(
                    rt.rng,
                    xm=float(allowance_cfg.pareto_xm),
                    alpha=float(allowance_cfg.pareto_alpha),
                )

                txns.append(
                    rt.txf.make(
                        TransactionDraft(
                            source=payer_acct,
                            destination=child_acct,
                            amount=amt,
                            timestamp=ts,
                            channel=ALLOWANCE,
                        )
                    )
                )

            jitter_days = as_int(cast(int | np.integer, gen.integers(-1, 2)))
            ts += timedelta(days=interval_days + jitter_days)

    return txns
