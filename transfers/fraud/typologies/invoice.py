from datetime import timedelta

from common.channels import FRAUD_INVOICE
from common.transactions import Transaction
from transfers.factory import TransactionDraft

from ..engine import IllicitContext
from ..rings import Plan
from .common import append_bounded_txn


def generate(
    ctx: IllicitContext,
    ring_plan: Plan,
    budget: int,
) -> list[Transaction]:
    if budget <= 0:
        return []
    if not ring_plan.fraud_accounts or not ctx.biller_accounts:
        return []

    rng = ctx.execution.rng
    base = ctx.window.start_date + timedelta(
        days=rng.int(0, max(1, ctx.window.days - 14))
    )
    weeks = max(1, min(6, ctx.window.days // 7))

    out: list[Transaction] = []
    for _ in range(rng.int(3, 10)):
        if len(out) >= budget:
            break

        src = rng.choice(ring_plan.fraud_accounts)
        dst = rng.choice(ctx.biller_accounts)

        ln = float(rng.gen.lognormal(mean=8.0, sigma=0.35))
        amt = round(ln / 10.0) * 10.0

        ts = base + timedelta(
            days=7 * rng.int(0, weeks),
            hours=rng.int(9, 18),
            minutes=rng.int(0, 60),
        )
        if not append_bounded_txn(
            ctx,
            out,
            budget,
            TransactionDraft(
                source=src,
                destination=dst,
                amount=amt,
                timestamp=ts,
                is_fraud=1,
                ring_id=ring_plan.ring_id,
                channel=FRAUD_INVOICE,
            ),
        ):
            break

    return out
