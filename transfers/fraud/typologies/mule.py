"""
Mule behavioral typology.

Generates the distinctive structural signature of money mule accounts:

1. HIGH FAN-IN: Many distinct sources (victims + external) send money to
   the mule within a short window. Legitimate hub accounts also have high
   degree, but spread over months. Mules concentrate 10-30 inbound sources
   in 1-2 weeks.

2. RAPID FORWARDING: Within hours of receiving funds, the mule forwards
   ~90-95% to the organizer. The small haircut (~5-10%) is the mule's fee.
   This creates a distinctive (inbound_amount - small_delta -> outbound)
   temporal pattern.

3. SHORT ACTIVE WINDOW: All mule activity concentrated in a 5-14 day burst.
   Before and after: the account is dormant or has only low-volume legitimate
   activity. This temporal concentration is measurable as the "burst score"
   feature in the ML pipeline.

4. ASYMMETRIC FLOW: Many small-to-medium inbound payments, fewer larger
   outbound payments. The aggregation/disaggregation pattern is a key AML
   indicator (FATF Red Flag Indicators, 2020).

This typology is grounded in:
- FATF Money Laundering Through Money Mules (2022)
- Europol EMSC operational data
- UK NCA "Money Mule" behavioral profiles
"""

from datetime import timedelta

from common.channels import FRAUD_MULE_IN, FRAUD_MULE_FORWARD
from common.transactions import Transaction
from math_models.amount_model import (
    FRAUD as FRAUD_MODEL,
    P2P as P2P_MODEL,
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
    """
    Generate mule behavioral patterns for a fraud ring.

    For each mule account:
      1. Generate 8-25 inbound transfers from victims (fan-in phase)
      2. For each inbound, generate a forwarding transfer to an organizer
         within 1-12 hours, retaining ~5-10% (forwarding phase)
      3. All activity within a tight burst window
    """
    if budget <= 0:
        return []

    rng = ctx.execution.rng

    if not ring_plan.mule_accounts:
        return []
    if not ring_plan.fraud_accounts and not ring_plan.victim_accounts:
        return []

    out: list[Transaction] = []

    base_date, burst_days = sample_burst_window(
        rng,
        start_date=ctx.window.start_date,
        total_days=ctx.window.days,
        tail_padding_days=14,
        min_burst_days=3,
        max_burst_days=11,
    )

    for mule_acct in ring_plan.mule_accounts:
        if len(out) >= budget:
            break

        inbound_count = rng.int(8, 26)

        source_pool: list[str] = list(ring_plan.victim_accounts)
        if ctx.biller_accounts:
            n_extra = min(len(ctx.biller_accounts), rng.int(3, 10))
            extra_sources = rng.choice_k(
                list(ctx.biller_accounts),
                n_extra,
                replace=False,
            )
            source_pool.extend(extra_sources)

        if not source_pool:
            continue

        if ring_plan.fraud_accounts:
            forward_targets = list(ring_plan.fraud_accounts)
        else:
            forward_targets = [m for m in ring_plan.mule_accounts if m != mule_acct]
            if not forward_targets:
                continue

        for inbound_idx in range(inbound_count):
            if len(out) >= budget:
                break

            src = source_pool[inbound_idx % len(source_pool)]
            if src == mule_acct:
                continue

            # Inbound amounts: mix of fraud-scale and inflated P2P-scale
            # to create realistic fan-in with varied amounts
            inbound_amt = FRAUD_MODEL.sample(rng)
            if rng.coin(0.40):
                inbound_amt = P2P_MODEL.sample(rng) * 3.0
            inbound_amt = round(max(50.0, inbound_amt), 2)

            inbound_ts = sample_timestamp(
                rng,
                base_date=base_date,
                max_days_offset=max(1, burst_days),
                min_hour=7,
                max_hour=22,
            )

            if not append_bounded_txn(
                ctx,
                out,
                budget,
                TransactionDraft(
                    source=src,
                    destination=mule_acct,
                    amount=inbound_amt,
                    timestamp=inbound_ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel=FRAUD_MULE_IN,
                ),
            ):
                break

            if len(out) >= budget:
                break

            # Forwarding: mule keeps 5-10% haircut, sends rest onward
            haircut = 0.05 + 0.05 * rng.float()
            forward_amt = round(inbound_amt * (1.0 - haircut), 2)
            if forward_amt < 10.0:
                continue

            forward_delay_hours = rng.int(1, 13)
            forward_ts = inbound_ts + timedelta(
                hours=forward_delay_hours,
                minutes=rng.int(0, 60),
            )

            forward_dst = rng.choice(forward_targets)

            if not append_bounded_txn(
                ctx,
                out,
                budget,
                TransactionDraft(
                    source=mule_acct,
                    destination=forward_dst,
                    amount=forward_amt,
                    timestamp=forward_ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel=FRAUD_MULE_FORWARD,
                ),
            ):
                break

    return out
