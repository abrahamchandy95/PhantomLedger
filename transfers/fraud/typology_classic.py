from common.transactions import Transaction
from math_models.amounts import cycle_amount, fraud_amount
from transfers.txns import TxnSpec

from .ring_plan import RingPlan
from .run_context import IllicitContext
from .schedule import random_ts, sample_burst_window
from .typology_common import append_bounded_txn


def inject_classic_typology(
    ctx: IllicitContext,
    ring_plan: RingPlan,
    budget: int,
) -> list[Transaction]:
    if budget <= 0:
        return []

    rng = ctx.execution.rng
    out: list[Transaction] = []
    base, burst_days = sample_burst_window(
        rng,
        start_date=ctx.window.start_date,
        days=ctx.window.days,
        base_tail_days=7,
        burst_min=2,
        burst_max_exclusive=6,
    )

    for victim_acct in ring_plan.victim_accounts:
        if len(out) >= budget:
            break

        if ring_plan.mule_accounts and rng.coin(0.75):
            mule = rng.choice(ring_plan.mule_accounts)
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
                    src=victim_acct,
                    dst=mule,
                    amt=fraud_amount(rng),
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel="fraud_classic",
                ),
            ):
                break

    for mule_acct in ring_plan.mule_accounts:
        if len(out) >= budget:
            break
        if not ring_plan.fraud_accounts:
            continue

        forwards = rng.int(2, 6)
        span = min(3, burst_days)

        for _ in range(forwards):
            if len(out) >= budget:
                break

            fraud_acct = rng.choice(ring_plan.fraud_accounts)
            ts = random_ts(
                rng,
                base=base,
                day_hi_exclusive=span,
                hour_lo=0,
                hour_hi_exclusive=24,
            )
            if not append_bounded_txn(
                ctx,
                out,
                budget,
                TxnSpec(
                    src=mule_acct,
                    dst=fraud_acct,
                    amt=fraud_amount(rng),
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel="fraud_classic",
                ),
            ):
                break

    nodes = ring_plan.fraud_accounts + ring_plan.mule_accounts
    if len(nodes) >= 3 and len(out) < budget:
        cycle_size = min(len(nodes), rng.int(3, 7))
        cycle_nodes = rng.choice_k(nodes, cycle_size, replace=False)
        passes = rng.int(2, 5)

        for _ in range(passes):
            if len(out) >= budget:
                break

            for idx, src in enumerate(cycle_nodes):
                if len(out) >= budget:
                    break

                dst = cycle_nodes[(idx + 1) % len(cycle_nodes)]
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
                        src=src,
                        dst=dst,
                        amt=cycle_amount(rng),
                        ts=ts,
                        is_fraud=1,
                        ring_id=ring_plan.ring_id,
                        channel="fraud_cycle",
                    ),
                ):
                    break

    return out
