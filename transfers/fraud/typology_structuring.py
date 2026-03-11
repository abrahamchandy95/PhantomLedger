from common.transactions import Transaction
from transfers.txns import TxnSpec

from .ring_plan import RingPlan
from .run_context import IllicitContext
from .schedule import random_ts, sample_burst_window
from .typology_common import append_bounded_txn


def inject_structuring_typology(
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
        burst_min=3,
        burst_max_exclusive=8,
    )

    if not ring_plan.mule_accounts and not ring_plan.fraud_accounts:
        return []

    target = (
        rng.choice(ring_plan.mule_accounts)
        if ring_plan.mule_accounts
        else rng.choice(ring_plan.fraud_accounts)
    )

    policy = ctx.structuring_policy
    threshold = float(policy.threshold)
    eps_min = float(policy.epsilon_min)
    eps_max = float(policy.epsilon_max)

    out: list[Transaction] = []

    for victim_acct in ring_plan.victim_accounts:
        if len(out) >= budget:
            break
        if not rng.coin(0.55):
            continue

        splits = rng.int(int(policy.splits_min), int(policy.splits_max) + 1)
        for _ in range(splits):
            if len(out) >= budget:
                break

            eps = eps_min + (eps_max - eps_min) * rng.float()
            amt = max(50.0, threshold - eps)
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
                    dst=target,
                    amt=amt,
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel="fraud_structuring",
                ),
            ):
                break

    return out
