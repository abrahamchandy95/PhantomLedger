from common.channels import FRAUD_FUNNEL_IN, FRAUD_FUNNEL_OUT
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
        min_burst_days=2,
        max_burst_days=6,
    )

    pool = ring_plan.fraud_accounts + ring_plan.mule_accounts
    if len(pool) < 2:
        return generate_classic(ctx, ring_plan, budget)

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
                    source=src,
                    destination=collector,
                    amount=FRAUD_MODEL.sample(rng),
                    timestamp=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel=FRAUD_FUNNEL_IN,
                ),
            ):
                break

    for _ in range(rng.int(6, 16)):
        if len(out) >= budget:
            break

        dst = rng.choice(cashouts) if cashouts else collector
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
                source=collector,
                destination=dst,
                amount=CYCLE_MODEL.sample(rng),
                timestamp=ts,
                is_fraud=1,
                ring_id=ring_plan.ring_id,
                channel=FRAUD_FUNNEL_OUT,
            ),
        ):
            break

    return out
