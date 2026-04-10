from common.channels import FRAUD_LAYERING_IN, FRAUD_LAYERING_HOP, FRAUD_LAYERING_OUT
from common.transactions import Transaction
from math_models.amount_model import (
    FRAUD as FRAUD_MODEL,
    FRAUD_CYCLE as CYCLE_MODEL,
)
from transfers.factory import TransactionDraft

from ..engine import IllicitContext
from ..rings import Plan
from ..schedule import sample_burst_window, sample_timestamp
from .classic import generate as generate_classic
from .common import append_bounded_txn


def generate(
    ctx: IllicitContext,
    ring_plan: Plan,
    budget: int,
) -> list[Transaction]:
    if budget <= 0:
        return []

    rng = ctx.execution.rng
    base_date, burst_days = sample_burst_window(
        rng,
        start_date=ctx.window.start_date,
        total_days=ctx.window.days,
        tail_padding_days=10,
        min_burst_days=3,
        max_burst_days=7,
    )

    nodes = ring_plan.mule_accounts + ring_plan.fraud_accounts
    if len(nodes) < 3 or not ring_plan.victim_accounts:
        return generate_classic(ctx, ring_plan, budget)

    rules = ctx.layering_rules
    hops = rng.int(int(rules.min_hops), int(rules.max_hops) + 1)

    chain = list(rng.choice_k(nodes, min(hops, len(nodes)), replace=False))
    while len(chain) < hops:
        chain.append(rng.choice(nodes))

    entry = chain[0]
    exit_acct = chain[-1]

    out: list[Transaction] = []

    for victim_acct in ring_plan.victim_accounts:
        if len(out) >= budget:
            break

        if rng.coin(0.60):
            ts = sample_timestamp(
                rng,
                base_date=base_date,
                max_days_offset=burst_days,
                min_hour=8,
                max_hour=22,
            )
            if not append_bounded_txn(
                ctx,
                out,
                budget,
                TransactionDraft(
                    source=victim_acct,
                    destination=entry,
                    amount=FRAUD_MODEL.sample(rng),
                    timestamp=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel=FRAUD_LAYERING_IN,
                ),
            ):
                break

    for src, dst in zip(chain[:-1], chain[1:]):
        if len(out) >= budget:
            break

        for _ in range(rng.int(1, 4)):
            if len(out) >= budget:
                break

            ts = sample_timestamp(
                rng,
                base_date=base_date,
                max_days_offset=burst_days,
                min_hour=0,
                max_hour=24,
            )
            if not append_bounded_txn(
                ctx,
                out,
                budget,
                TransactionDraft(
                    source=src,
                    destination=dst,
                    amount=CYCLE_MODEL.sample(rng),
                    timestamp=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel=FRAUD_LAYERING_HOP,
                ),
            ):
                break

    if len(out) < budget:
        cashout = (
            rng.choice(ring_plan.fraud_accounts)
            if ring_plan.fraud_accounts
            else rng.choice(nodes)
        )
        ts = sample_timestamp(
            rng,
            base_date=base_date,
            max_days_offset=burst_days,
            min_hour=0,
            max_hour=24,
        )
        _ = append_bounded_txn(
            ctx,
            out,
            budget,
            TransactionDraft(
                source=exit_acct,
                destination=cashout,
                amount=FRAUD_MODEL.sample(rng),
                timestamp=ts,
                is_fraud=1,
                ring_id=ring_plan.ring_id,
                channel=FRAUD_LAYERING_OUT,
            ),
        )

    return out
