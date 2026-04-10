from datetime import timedelta

from common.channels import CAMOUFLAGE_BILL, CAMOUFLAGE_P2P, CAMOUFLAGE_SALARY
from common.timeline import active_months
from common.transactions import Transaction
from math_models.amount_model import (
    BILL as BILL_MODEL,
    P2P as P2P_MODEL,
    SALARY as SALARY_MODEL,
)
from transfers.factory import TransactionDraft

from .engine import CamouflageContext
from .rings import Plan


def generate(
    ctx: CamouflageContext,
    ring_plan: Plan,
) -> list[Transaction]:
    """Generates legitimate-looking transactions to hide fraud activity in ring accounts."""
    ring_accounts = ring_plan.participant_accounts
    if not ring_accounts:
        return []

    rng = ctx.execution.rng
    txf = ctx.execution.txf
    rates = ctx.rates
    start_date = ctx.window.start_date
    days = ctx.window.days

    txns: list[Transaction] = []

    # 1. Camouflage: Monthly Bills
    if ctx.accounts.biller_accounts and float(rates.bill_monthly_p) > 0.0:
        for pay_day in active_months(start_date, days):
            for acct in ring_accounts:
                if rng.coin(float(rates.bill_monthly_p)):
                    dst = rng.choice(ctx.accounts.biller_accounts)

                    offset_days = rng.int(0, 5)
                    offset_hours = rng.int(7, 22)
                    offset_mins = rng.int(0, 60)

                    ts = pay_day + timedelta(
                        days=offset_days,
                        hours=offset_hours,
                        minutes=offset_mins,
                    )

                    txns.append(
                        txf.make(
                            TransactionDraft(
                                source=acct,
                                destination=dst,
                                amount=BILL_MODEL.sample(rng),
                                timestamp=ts,
                                is_fraud=0,
                                ring_id=-1,
                                channel=CAMOUFLAGE_BILL,
                            )
                        )
                    )

    # 2. Camouflage: Small Daily P2P
    for day in range(days):
        day_start = start_date + timedelta(days=day)
        for acct in ring_accounts:
            if rng.coin(float(rates.small_p2p_per_day_p)):
                dst = rng.choice(ctx.accounts.all_accounts)
                if dst == acct:
                    continue

                offset_hours = rng.int(0, 24)
                offset_mins = rng.int(0, 60)

                ts = day_start + timedelta(
                    hours=offset_hours,
                    minutes=offset_mins,
                )

                txns.append(
                    txf.make(
                        TransactionDraft(
                            source=acct,
                            destination=dst,
                            amount=P2P_MODEL.sample(rng),
                            timestamp=ts,
                            is_fraud=0,
                            ring_id=-1,
                            channel=CAMOUFLAGE_P2P,
                        )
                    )
                )

    # 3. Camouflage: Inbound Salary
    if ctx.accounts.employers and float(rates.salary_inbound_p) > 0.0:
        for acct in ring_accounts:
            if rng.coin(float(rates.salary_inbound_p)):
                src = rng.choice(ctx.accounts.employers)

                offset_days = rng.int(0, max(1, days))
                offset_hours = rng.int(8, 18)
                offset_mins = rng.int(0, 60)

                ts = start_date + timedelta(
                    days=offset_days,
                    hours=offset_hours,
                    minutes=offset_mins,
                )

                txns.append(
                    txf.make(
                        TransactionDraft(
                            source=src,
                            destination=acct,
                            amount=SALARY_MODEL.sample(rng),
                            timestamp=ts,
                            is_fraud=0,
                            ring_id=-1,
                            channel=CAMOUFLAGE_SALARY,
                        )
                    )
                )

    return txns
