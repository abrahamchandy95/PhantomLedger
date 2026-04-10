from common.channels import FRAUD_STRUCTURING
from common.transactions import Transaction
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
    if budget <= 0:
        return []

    rng = ctx.execution.rng
    base_date, burst_days = sample_burst_window(
        rng,
        start_date=ctx.window.start_date,
        total_days=ctx.window.days,
        tail_padding_days=10,
        min_burst_days=3,
        max_burst_days=8,
    )

    if not ring_plan.mule_accounts and not ring_plan.fraud_accounts:
        return []

    target = (
        rng.choice(ring_plan.mule_accounts)
        if ring_plan.mule_accounts
        else rng.choice(ring_plan.fraud_accounts)
    )

    rules = ctx.structuring_rules
    threshold = float(rules.threshold)
    eps_min = float(rules.epsilon_min)
    eps_max = float(rules.epsilon_max)

    out: list[Transaction] = []

    for victim_acct in ring_plan.victim_accounts:
        if len(out) >= budget:
            break
        if not rng.coin(0.55):
            continue

        splits = rng.int(int(rules.splits_min), int(rules.splits_max) + 1)
        for _ in range(splits):
            if len(out) >= budget:
                break

            eps = eps_min + (eps_max - eps_min) * rng.float()
            amt = max(50.0, threshold - eps)
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
                    destination=target,
                    amount=amt,
                    timestamp=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel=FRAUD_STRUCTURING,
                ),
            ):
                break

    return out
