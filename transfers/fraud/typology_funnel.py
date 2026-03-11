from common.transactions import Transaction
from math_models.amounts import cycle_amount, fraud_amount
from transfers.txns import TxnSpec

from .ring_plan import RingPlan
from .run_context import IllicitContext
from .schedule import random_ts, sample_burst_window
from .typology_classic import inject_classic_typology
from .typology_common import append_bounded_txn


def inject_funnel_typology(
    ctx: IllicitContext,
    ring_plan: RingPlan,
    budget: int,
) -> list[Transaction]:
    if budget <= 0:
        return []

    rng = ctx.execution.rng
    base, burst_days = sample_burst_window(
        rng,
        start_date=ctx.window.start_date,
        days=ctx.window.days,
        base_tail_days=10,
        burst_min=2,
        burst_max_exclusive=6,
    )

    pool = ring_plan.fraud_accounts + ring_plan.mule_accounts
    if len(pool) < 2:
        return inject_classic_typology(ctx, ring_plan, budget)

    collector = rng.choice(pool)
    cashouts = rng.choice_k(pool, min(4, len(pool)), replace=False)
    if collector in cashouts and len(cashouts) > 1:
        cashouts = [acct for acct in cashouts if acct != collector]

    out: list[Transaction] = []
    sources = ring_plan.victim_accounts + ring_plan.mule_accounts

    for src in sources:
        if len(out) >= budget:
            break

        if rng.coin(0.55):
            ts = random_ts(
                rng,
                base=base,
                day_hi_exclusive=burst_days,
                hour_lo=8,
                hour_hi_exclusive=22,
            )
            if not append_bounded_txn(
                ctx,
                out,
                budget,
                TxnSpec(
                    src=src,
                    dst=collector,
                    amt=fraud_amount(rng),
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel="fraud_funnel_in",
                ),
            ):
                break

    for _ in range(rng.int(6, 16)):
        if len(out) >= budget:
            break

        dst = rng.choice(cashouts) if cashouts else collector
        ts = random_ts(
            rng,
            base=base,
            day_hi_exclusive=burst_days,
            hour_lo=0,
            hour_hi_exclusive=24,
        )
        if not append_bounded_txn(
            ctx,
            out,
            budget,
            TxnSpec(
                src=collector,
                dst=dst,
                amt=cycle_amount(rng),
                ts=ts,
                is_fraud=1,
                ring_id=ring_plan.ring_id,
                channel="fraud_funnel_out",
            ),
        ):
            break

    return out
