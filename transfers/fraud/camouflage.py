from datetime import datetime, timedelta

from common.channels import CAMOUFLAGE_BILL, CAMOUFLAGE_P2P, CAMOUFLAGE_SALARY
from common.random import Rng
from common.timeline import active_months
from common.transactions import Transaction
from math_models.amount_model import (
    BILL as BILL_MODEL,
    P2P as P2P_MODEL,
    SALARY as SALARY_MODEL,
)
import relationships.recurring as recurring_model
from transfers.factory import TransactionDraft

from .engine import CamouflageContext
from .rings import Plan


_PAYROLL_POLICY = recurring_model.Policy()


def _next_weekday_on_or_after(ts: datetime, weekday: int) -> datetime:
    delta = (weekday - ts.weekday()) % 7
    return datetime(ts.year, ts.month, ts.day) + timedelta(days=delta)


def _sample_camouflage_payroll_profile(rng: Rng) -> recurring_model.PayrollProfile:
    cadence: recurring_model.PayCadence = rng.weighted_choice(
        [
            recurring_model.PayCadence.WEEKLY,
            recurring_model.PayCadence.BIWEEKLY,
            recurring_model.PayCadence.SEMIMONTHLY,
            recurring_model.PayCadence.MONTHLY,
        ],
        [
            float(_PAYROLL_POLICY.weekly_pay_weight),
            float(_PAYROLL_POLICY.biweekly_pay_weight),
            float(_PAYROLL_POLICY.semimonthly_pay_weight),
            float(_PAYROLL_POLICY.monthly_pay_weight),
        ],
    )

    weekday = int(_PAYROLL_POLICY.payroll_default_weekday)
    if cadence in {
        recurring_model.PayCadence.WEEKLY,
        recurring_model.PayCadence.BIWEEKLY,
    } and rng.coin(0.25):
        weekday = 3 if weekday == 4 else 4

    anchor = _next_weekday_on_or_after(datetime(2025, 1, 1), weekday)

    if cadence is recurring_model.PayCadence.BIWEEKLY and rng.coin(0.5):
        anchor += timedelta(days=7)

    lag_max = max(0, int(_PAYROLL_POLICY.posting_lag_days_max))
    posting_lag_days: int = 0 if lag_max == 0 else int(rng.int(0, lag_max + 1))

    if cadence is recurring_model.PayCadence.SEMIMONTHLY:
        semimonthly_days: tuple[int, int] = (1, 15) if rng.coin(0.35) else (15, 31)
        return recurring_model.PayrollProfile(
            cadence=cadence,
            anchor_date=anchor,
            weekday=weekday,
            semimonthly_days=semimonthly_days,
            posting_lag_days=posting_lag_days,
        )

    if cadence is recurring_model.PayCadence.MONTHLY:
        monthly_day = int(rng.choice([28, 30, 31]))
        return recurring_model.PayrollProfile(
            cadence=cadence,
            anchor_date=anchor,
            weekday=weekday,
            monthly_day=monthly_day,
            posting_lag_days=posting_lag_days,
        )

    return recurring_model.PayrollProfile(
        cadence=cadence,
        anchor_date=anchor,
        weekday=weekday,
        posting_lag_days=posting_lag_days,
    )


def generate(
    ctx: CamouflageContext,
    ring_plan: Plan,
) -> list[Transaction]:
    """Generate legitimate-looking transactions to hide fraud activity in ring accounts."""
    ring_accounts = ring_plan.participant_accounts
    if not ring_accounts:
        return []

    rng: Rng = ctx.execution.rng
    txf = ctx.execution.txf
    rates = ctx.rates
    start_date = ctx.window.start_date
    days = int(ctx.window.days)
    window_end_excl = start_date + timedelta(days=days)

    txns: list[Transaction] = []

    # 1. Camouflage: monthly bills
    if ctx.accounts.biller_accounts and float(rates.bill_monthly_p) > 0.0:
        for pay_day in active_months(start_date, days):
            for acct in ring_accounts:
                if not rng.coin(float(rates.bill_monthly_p)):
                    continue

                dst = rng.choice(ctx.accounts.biller_accounts)

                offset_days = rng.int(0, 5)
                offset_hours = rng.int(7, 22)
                offset_mins = rng.int(0, 60)

                ts = pay_day + timedelta(
                    days=offset_days,
                    hours=offset_hours,
                    minutes=offset_mins,
                )

                if ts >= window_end_excl:
                    continue

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

    # 2. Camouflage: small daily P2P
    for day in range(days):
        day_start = start_date + timedelta(days=day)
        for acct in ring_accounts:
            if not rng.coin(float(rates.small_p2p_per_day_p)):
                continue

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

    # 3. Camouflage: recurring inbound salary
    if ctx.accounts.employers and float(rates.salary_inbound_p) > 0.0:
        for acct in ring_accounts:
            if not rng.coin(float(rates.salary_inbound_p)):
                continue

            src = rng.choice(ctx.accounts.employers)
            profile = _sample_camouflage_payroll_profile(rng)
            annual_salary = float(SALARY_MODEL.sample(rng)) * 12.0

            for pay_date in recurring_model.employment.paydates_for_profile(
                profile,
                start=start_date,
                end_excl=window_end_excl,
            ):
                ts = pay_date + timedelta(
                    days=profile.posting_lag_days,
                    hours=rng.int(6, 11),
                    minutes=rng.int(0, 60),
                )

                if ts < start_date or ts >= window_end_excl:
                    continue

                periods = recurring_model.employment.pay_periods_in_year(
                    profile,
                    pay_date.year,
                )
                amount = round(max(50.0, annual_salary / float(periods)), 2)

                txns.append(
                    txf.make(
                        TransactionDraft(
                            source=src,
                            destination=acct,
                            amount=amount,
                            timestamp=ts,
                            is_fraud=0,
                            ring_id=-1,
                            channel=CAMOUFLAGE_SALARY,
                        )
                    )
                )

    return txns
