from common.transactions import Transaction
from math_models.amounts import cycle_amount, fraud_amount
from transfers.txns import TxnSpec

from .ring_plan import RingPlan
from .run_context import IllicitContext
from .schedule import random_ts, sample_burst_window
from .typology_classic import inject_classic_typology
from .typology_common import append_bounded_txn


def inject_layering_typology(
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
        burst_max_exclusive=7,
    )

    nodes = ring_plan.mule_accounts + ring_plan.fraud_accounts
    if len(nodes) < 3 or not ring_plan.victim_accounts:
        return inject_classic_typology(ctx, ring_plan, budget)

    policy = ctx.layering_policy
    hops = rng.int(int(policy.min_hops), int(policy.max_hops) + 1)

    chain = rng.choice_k(nodes, min(hops, len(nodes)), replace=False)
    while len(chain) < hops:
        chain.append(rng.choice(nodes))

    entry = chain[0]
    exit_ = chain[-1]

    out: list[Transaction] = []

    for victim_acct in ring_plan.victim_accounts:
        if len(out) >= budget:
            break

        if rng.coin(0.60):
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
                    dst=entry,
                    amt=fraud_amount(rng),
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel="fraud_layering_in",
                ),
            ):
                break

    for src, dst in zip(chain[:-1], chain[1:]):
        if len(out) >= budget:
            break

        for _ in range(rng.int(1, 4)):
            if len(out) >= budget:
                break

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
                    channel="fraud_layering_hop",
                ),
            ):
                break

    if len(out) < budget:
        cashout = (
            rng.choice(ring_plan.fraud_accounts)
            if ring_plan.fraud_accounts
            else rng.choice(nodes)
        )
        ts = random_ts(
            rng,
            base=base,
            day_hi_exclusive=burst_days,
            hour_lo=0,
            hour_hi_exclusive=24,
        )
        _ = append_bounded_txn(
            ctx,
            out,
            budget,
            TxnSpec(
                src=exit_,
                dst=cashout,
                amt=fraud_amount(rng),
                ts=ts,
                is_fraud=1,
                ring_id=ring_plan.ring_id,
                channel="fraud_layering_out",
            ),
        )

    return out
