from dataclasses import dataclass
from datetime import datetime, timedelta

from common.random import SeedBank
from common.types import Txn
from transfers.txns import TxnSpec

from .models import CreditLifecyclePolicy, CreditLifecycleRequest
from .schedule import effective_due_cutoff, iter_cycle_closes, payment_received_on_time
from .statement import (
    choose_manual_payment_amount,
    days_frac,
    finite_interest_amount,
    integrated_avg_balance,
    min_payment,
    payment_timestamp,
    sample_credit_event_for_purchase,
)


_DEFAULT_CARD_APR = 0.22
_DEFAULT_CYCLE_DAY = 15


@dataclass(frozen=True, slots=True)
class _CardRuntime:
    card: str
    pay_from: str
    apr: float
    cycle_day: int
    autopay_mode: int


def _collect_purchases_by_card(
    request: CreditLifecycleRequest,
    start: datetime,
    end_excl: datetime,
) -> dict[str, list[Txn]]:
    card_set = set(request.cards.card_accounts)
    purchases_by_card: dict[str, list[Txn]] = {
        card: [] for card in request.cards.card_accounts
    }

    for txn in request.existing_txns:
        if txn.channel != "card_purchase":
            continue
        if txn.src_acct in card_set and start <= txn.ts < end_excl:
            purchases_by_card[txn.src_acct].append(txn)

    return purchases_by_card


def _card_runtime_for(
    request: CreditLifecycleRequest,
    card: str,
) -> _CardRuntime | None:
    owner = request.cards.owner_for_card.get(card)
    if owner is None:
        return None

    pay_from = request.primary_acct_for_person.get(owner)
    if pay_from is None:
        return None

    apr = float(request.cards.apr_by_card.get(card, _DEFAULT_CARD_APR))
    cycle_day = int(request.cards.cycle_day_by_card.get(card, _DEFAULT_CYCLE_DAY))
    autopay_mode = int(request.cards.autopay_mode_by_card.get(card, 0))

    return _CardRuntime(
        card=card,
        pay_from=pay_from,
        apr=apr,
        cycle_day=cycle_day,
        autopay_mode=autopay_mode,
    )


def generate_credit_card_lifecycle_txns(
    policy: CreditLifecyclePolicy,
    request: CreditLifecycleRequest,
) -> list[Txn]:
    """
    Reads existing card_purchase txns and generates:
      - refunds / chargebacks (external -> card)
      - interest (card -> issuer)
      - late fees (card -> issuer)
      - payments (deposit -> card)
    """
    policy.validate()

    if not request.cards.card_accounts:
        return []

    start = request.window.start_date()
    end_excl = start + timedelta(days=int(request.window.days))

    purchases_by_card = _collect_purchases_by_card(request, start, end_excl)
    out: list[Txn] = []

    seedbank = SeedBank(request.base_seed)
    lifecycle_gen = seedbank.generator("cc_lifecycle")

    for card, purchases in purchases_by_card.items():
        if not purchases:
            continue

        runtime = _card_runtime_for(request, card)
        if runtime is None:
            continue

        purchases.sort(key=lambda txn: txn.ts)

        closes = iter_cycle_closes(start, end_excl, runtime.cycle_day)
        if not closes:
            continue

        balance = 0.0
        in_grace = True

        scheduled_credits: list[Txn] = []
        credit_idx = 0
        purchase_idx = 0
        last_close = start

        for close in closes:
            cycle_start = last_close
            cycle_end = close

            cycle_events: list[Txn] = []

            while (
                purchase_idx < len(purchases) and purchases[purchase_idx].ts < cycle_end
            ):
                purchase = purchases[purchase_idx]
                cycle_events.append(purchase)

                credit_idx += 1
                credit_txn = sample_credit_event_for_purchase(
                    policy,
                    lifecycle_gen,
                    card=card,
                    credit_idx=credit_idx,
                    purchase=purchase,
                    end_excl=end_excl,
                    txf=request.txf,
                )
                if credit_txn is not None:
                    scheduled_credits.append(credit_txn)
                    out.append(credit_txn)

                purchase_idx += 1

            if scheduled_credits:
                keep: list[Txn] = []
                for credit_txn in scheduled_credits:
                    if cycle_start <= credit_txn.ts < cycle_end:
                        cycle_events.append(credit_txn)
                    else:
                        keep.append(credit_txn)
                scheduled_credits = keep

            cycle_events.sort(key=lambda txn: txn.ts)

            avg_balance, balance_end = integrated_avg_balance(
                card,
                balance,
                cycle_events,
                cycle_start,
                cycle_end,
            )
            balance = balance_end

            if not in_grace:
                interval_days = days_frac(cycle_start, cycle_end)
                debt_avg = max(0.0, -float(avg_balance))
                interest_raw = (
                    debt_avg * (float(runtime.apr) / 365.0) * float(interval_days)
                )
                interest = finite_interest_amount(interest_raw)

                if interest is not None:
                    interest_txn = request.txf.make(
                        TxnSpec(
                            src=card,
                            dst=request.issuer_acct,
                            amt=interest,
                            ts=close + timedelta(minutes=15),
                            channel="cc_interest",
                        )
                    )
                    out.append(interest_txn)
                    balance -= float(interest)

            statement_abs = max(0.0, -balance)
            if statement_abs <= 0.01:
                in_grace = True
                last_close = close
                continue

            min_due = round(min_payment(policy, statement_abs), 2)
            due = close + timedelta(days=int(policy.grace_days), hours=17)

            if runtime.autopay_mode == 2:
                pay_amt = float(statement_abs)
                pay_ts = payment_timestamp(policy, request.rng, due, is_autopay=True)
            elif runtime.autopay_mode == 1:
                pay_amt = float(min_due)
                pay_ts = payment_timestamp(policy, request.rng, due, is_autopay=True)
            else:
                pay_amt = choose_manual_payment_amount(
                    policy,
                    request.rng,
                    statement_abs,
                    min_due,
                )
                pay_ts = payment_timestamp(policy, request.rng, due, is_autopay=False)

            pay_amt = round(max(0.0, float(pay_amt)), 2)

            paid_by_due = (pay_amt >= min_due - 1e-6) and payment_received_on_time(
                pay_ts,
                due,
            )
            paid_full_by_due = (
                pay_amt >= statement_abs - 1e-6
            ) and payment_received_on_time(
                pay_ts,
                due,
            )

            if pay_amt > 0.0 and pay_ts < end_excl:
                payment_txn = request.txf.make(
                    TxnSpec(
                        src=runtime.pay_from,
                        dst=card,
                        amt=pay_amt,
                        ts=pay_ts,
                        channel="cc_payment",
                    )
                )
                out.append(payment_txn)
                balance += float(pay_amt)

            if not paid_by_due and float(policy.late_fee_amount) > 0.0:
                fee_ts = min(
                    end_excl - timedelta(seconds=1),
                    effective_due_cutoff(due) + timedelta(days=1, hours=10),
                )
                fee = round(float(policy.late_fee_amount), 2)
                fee_txn = request.txf.make(
                    TxnSpec(
                        src=card,
                        dst=request.issuer_acct,
                        amt=fee,
                        ts=fee_ts,
                        channel="cc_late_fee",
                    )
                )
                out.append(fee_txn)
                balance -= float(fee)

            in_grace = bool(paid_full_by_due)
            last_close = close

    out.sort(
        key=lambda txn: (
            txn.ts,
            txn.src_acct,
            txn.dst_acct,
            float(txn.amount),
            txn.channel or "",
        )
    )
    return out
