from common.channels import FRAUD_CLASSIC, FRAUD_CYCLE
from common.transactions import Transaction
from math_models.amount_model import (
    FRAUD as FRAUD_MODEL,
    FRAUD_CYCLE as CYCLE_MODEL,
)
from transfers.factory import TransactionDraft

from ..engine import IllicitContext
from ..rings import Plan
from ..schedule import sample_burst_window, sample_timestamp
from .common import append_bounded_txn


def generate(
    ctx: IllicitContext,
    ring_plan: Plan,
    budget: int,
) -> list[Transaction]:
    """Generates a classic fraud-ring pattern with victim->mule, mule->fraud, and cycle hops."""
    if budget <= 0:
        return []

    rng = ctx.execution.rng
    out: list[Transaction] = []

    base_date, burst_days = sample_burst_window(
        rng,
        start_date=ctx.window.start_date,
        total_days=ctx.window.days,
        tail_padding_days=7,
        min_burst_days=2,
        max_burst_days=6,
    )

    for victim_acct in ring_plan.victim_accounts:
        if len(out) >= budget:
            break

        if ring_plan.mule_accounts and rng.coin(0.75):
            mule_acct = rng.choice(ring_plan.mule_accounts)
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
                    destination=mule_acct,
                    amount=FRAUD_MODEL.sample(rng),
                    timestamp=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel=FRAUD_CLASSIC,
                ),
            ):
                break

    for mule_acct in ring_plan.mule_accounts:
        if len(out) >= budget:
            break
        if not ring_plan.fraud_accounts:
            continue

        forwards = rng.int(2, 6)
        span_days = min(3, burst_days)

        for _ in range(forwards):
            if len(out) >= budget:
                break

            fraud_acct = rng.choice(ring_plan.fraud_accounts)
            ts = sample_timestamp(
                rng,
                base_date=base_date,
                max_days_offset=span_days,
                min_hour=0,
                max_hour=24,
            )
            if not append_bounded_txn(
                ctx,
                out,
                budget,
                TransactionDraft(
                    source=mule_acct,
                    destination=fraud_acct,
                    amount=FRAUD_MODEL.sample(rng),
                    timestamp=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel=FRAUD_CLASSIC,
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

            for idx, src_acct in enumerate(cycle_nodes):
                if len(out) >= budget:
                    break

                dst_acct = cycle_nodes[(idx + 1) % len(cycle_nodes)]
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
                        source=src_acct,
                        destination=dst_acct,
                        amount=CYCLE_MODEL.sample(rng),
                        timestamp=ts,
                        is_fraud=1,
                        ring_id=ring_plan.ring_id,
                        channel=FRAUD_CYCLE,
                    ),
                ):
                    break

    return out
