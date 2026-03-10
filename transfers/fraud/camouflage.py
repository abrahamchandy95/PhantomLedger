from datetime import timedelta

from common.temporal import iter_window_month_starts
from common.types import Txn
from math_models.amounts import bill_amount, p2p_amount, salary_amount
from transfers.txns import TxnSpec

from .ring_plan import RingPlan
from .run_context import CamouflageContext


def inject_camouflage(
    ctx: CamouflageContext,
    ring_plan: RingPlan,
) -> list[Txn]:
    ring_accounts = ring_plan.participant_accounts
    if not ring_accounts:
        return []

    rng = ctx.execution.rng
    txf = ctx.execution.txf
    policy = ctx.policy
    start_date = ctx.window.start_date
    days = ctx.window.days

    out: list[Txn] = []

    if ctx.accounts.biller_accounts and float(policy.bill_monthly_p) > 0.0:
        for pay_day in iter_window_month_starts(start_date, days):
            for acct in ring_accounts:
                if rng.coin(float(policy.bill_monthly_p)):
                    dst = rng.choice(ctx.accounts.biller_accounts)
                    ts = pay_day + timedelta(
                        days=rng.int(0, 5),
                        hours=rng.int(7, 22),
                        minutes=rng.int(0, 60),
                    )
                    out.append(
                        txf.make(
                            TxnSpec(
                                src=acct,
                                dst=dst,
                                amt=bill_amount(rng),
                                ts=ts,
                                is_fraud=0,
                                ring_id=ring_plan.ring_id,
                                channel="camouflage_bill",
                            )
                        )
                    )

    for day in range(days):
        day_start = start_date + timedelta(days=day)
        for acct in ring_accounts:
            if rng.coin(float(policy.small_p2p_per_day_p)):
                dst = rng.choice(ctx.accounts.all_accounts)
                if dst == acct:
                    continue
                ts = day_start + timedelta(
                    hours=rng.int(0, 24),
                    minutes=rng.int(0, 60),
                )
                out.append(
                    txf.make(
                        TxnSpec(
                            src=acct,
                            dst=dst,
                            amt=p2p_amount(rng),
                            ts=ts,
                            is_fraud=0,
                            ring_id=ring_plan.ring_id,
                            channel="camouflage_p2p",
                        )
                    )
                )

    if ctx.accounts.employers and float(policy.salary_inbound_p) > 0.0:
        for acct in ring_accounts:
            if rng.coin(float(policy.salary_inbound_p)):
                src = rng.choice(ctx.accounts.employers)
                ts = start_date + timedelta(
                    days=rng.int(0, max(1, days)),
                    hours=rng.int(8, 18),
                    minutes=rng.int(0, 60),
                )
                out.append(
                    txf.make(
                        TxnSpec(
                            src=src,
                            dst=acct,
                            amt=salary_amount(rng),
                            ts=ts,
                            is_fraud=0,
                            ring_id=ring_plan.ring_id,
                            channel="camouflage_salary",
                        )
                    )
                )

    return out
