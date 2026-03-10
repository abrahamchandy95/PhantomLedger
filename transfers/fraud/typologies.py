from datetime import timedelta

from common.types import Txn
from math_models.amounts import cycle_amount, fraud_amount
from transfers.txns import TxnSpec

from .policy import Typology
from .ring_plan import RingPlan
from .run_context import IllicitContext
from .schedule import random_ts, sample_burst_window


def inject_illicit_for_ring(
    ctx: IllicitContext,
    ring_plan: RingPlan,
    typology: Typology,
    budget: int,
) -> list[Txn]:
    match typology:
        case "classic":
            return _inject_classic(ctx, ring_plan, budget)
        case "layering":
            return _inject_layering(ctx, ring_plan, budget)
        case "funnel":
            return _inject_funnel(ctx, ring_plan, budget)
        case "structuring":
            return _inject_structuring(ctx, ring_plan, budget)
        case "invoice":
            return _inject_invoice_like(ctx, ring_plan, budget)


def _append_bounded(
    ctx: IllicitContext,
    out: list[Txn],
    budget: int,
    spec: TxnSpec,
) -> bool:
    if len(out) >= budget:
        return False
    out.append(ctx.execution.txf.make(spec))
    return True


def _inject_classic(
    ctx: IllicitContext,
    ring_plan: RingPlan,
    budget: int,
) -> list[Txn]:
    if budget <= 0:
        return []

    rng = ctx.execution.rng
    out: list[Txn] = []
    base, burst_days = sample_burst_window(
        rng,
        start_date=ctx.window.start_date,
        days=ctx.window.days,
        base_tail_days=7,
        burst_min=2,
        burst_max_exclusive=6,
    )

    for victim_acct in ring_plan.victim_accounts:
        if len(out) >= budget:
            break
        if ring_plan.mule_accounts and rng.coin(0.75):
            mule = rng.choice(ring_plan.mule_accounts)
            ts = random_ts(
                rng,
                base=base,
                day_hi_exclusive=burst_days,
                hour_lo=8,
                hour_hi_exclusive=22,
            )
            if not _append_bounded(
                ctx,
                out,
                budget,
                TxnSpec(
                    src=victim_acct,
                    dst=mule,
                    amt=fraud_amount(rng),
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel="fraud_classic",
                ),
            ):
                break

    for mule_acct in ring_plan.mule_accounts:
        if len(out) >= budget:
            break
        if not ring_plan.fraud_accounts:
            continue

        forwards = rng.int(2, 6)
        span = min(3, burst_days)

        for _ in range(forwards):
            if len(out) >= budget:
                break
            fraud_acct = rng.choice(ring_plan.fraud_accounts)
            ts = random_ts(
                rng,
                base=base,
                day_hi_exclusive=span,
                hour_lo=0,
                hour_hi_exclusive=24,
            )
            if not _append_bounded(
                ctx,
                out,
                budget,
                TxnSpec(
                    src=mule_acct,
                    dst=fraud_acct,
                    amt=fraud_amount(rng),
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel="fraud_classic",
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
            for idx, src in enumerate(cycle_nodes):
                if len(out) >= budget:
                    break
                dst = cycle_nodes[(idx + 1) % len(cycle_nodes)]
                ts = random_ts(
                    rng,
                    base=base,
                    day_hi_exclusive=burst_days,
                    hour_lo=0,
                    hour_hi_exclusive=24,
                )
                if not _append_bounded(
                    ctx,
                    out,
                    budget,
                    TxnSpec(
                        src=src,
                        dst=dst,
                        amt=cycle_amount(rng),
                        ts=ts,
                        is_fraud=1,
                        ring_id=ring_plan.ring_id,
                        channel="fraud_cycle",
                    ),
                ):
                    break

    return out


def _inject_layering(
    ctx: IllicitContext,
    ring_plan: RingPlan,
    budget: int,
) -> list[Txn]:
    if budget <= 0:
        return []

    rng = ctx.execution.rng
    base, burst_days = sample_burst_window(
        rng,
        start_date=ctx.window.start_date,
        days=ctx.window.days,
        base_tail_days=10,
        burst_min=3,
        burst_max_exclusive=7,
    )

    nodes = ring_plan.mule_accounts + ring_plan.fraud_accounts
    if len(nodes) < 3 or not ring_plan.victim_accounts:
        return _inject_classic(ctx, ring_plan, budget)

    policy = ctx.layering_policy
    hops = rng.int(int(policy.min_hops), int(policy.max_hops) + 1)

    chain = rng.choice_k(nodes, min(hops, len(nodes)), replace=False)
    while len(chain) < hops:
        chain.append(rng.choice(nodes))

    entry = chain[0]
    exit_ = chain[-1]

    out: list[Txn] = []

    for victim_acct in ring_plan.victim_accounts:
        if len(out) >= budget:
            break
        if rng.coin(0.60):
            ts = random_ts(
                rng,
                base=base,
                day_hi_exclusive=burst_days,
                hour_lo=8,
                hour_hi_exclusive=22,
            )
            if not _append_bounded(
                ctx,
                out,
                budget,
                TxnSpec(
                    src=victim_acct,
                    dst=entry,
                    amt=fraud_amount(rng),
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel="fraud_layering_in",
                ),
            ):
                break

    for src, dst in zip(chain[:-1], chain[1:]):
        if len(out) >= budget:
            break

        for _ in range(rng.int(1, 4)):
            if len(out) >= budget:
                break
            ts = random_ts(
                rng,
                base=base,
                day_hi_exclusive=burst_days,
                hour_lo=0,
                hour_hi_exclusive=24,
            )
            if not _append_bounded(
                ctx,
                out,
                budget,
                TxnSpec(
                    src=src,
                    dst=dst,
                    amt=cycle_amount(rng),
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel="fraud_layering_hop",
                ),
            ):
                break

    if len(out) < budget:
        cashout = (
            rng.choice(ring_plan.fraud_accounts)
            if ring_plan.fraud_accounts
            else rng.choice(nodes)
        )
        ts = random_ts(
            rng,
            base=base,
            day_hi_exclusive=burst_days,
            hour_lo=0,
            hour_hi_exclusive=24,
        )
        _ = _append_bounded(
            ctx,
            out,
            budget,
            TxnSpec(
                src=exit_,
                dst=cashout,
                amt=fraud_amount(rng),
                ts=ts,
                is_fraud=1,
                ring_id=ring_plan.ring_id,
                channel="fraud_layering_out",
            ),
        )

    return out


def _inject_funnel(
    ctx: IllicitContext,
    ring_plan: RingPlan,
    budget: int,
) -> list[Txn]:
    if budget <= 0:
        return []

    rng = ctx.execution.rng
    base, burst_days = sample_burst_window(
        rng,
        start_date=ctx.window.start_date,
        days=ctx.window.days,
        base_tail_days=10,
        burst_min=2,
        burst_max_exclusive=6,
    )

    pool = ring_plan.fraud_accounts + ring_plan.mule_accounts
    if len(pool) < 2:
        return _inject_classic(ctx, ring_plan, budget)

    collector = rng.choice(pool)
    cashouts = rng.choice_k(pool, min(4, len(pool)), replace=False)
    if collector in cashouts and len(cashouts) > 1:
        cashouts = [acct for acct in cashouts if acct != collector]

    out: list[Txn] = []
    sources = ring_plan.victim_accounts + ring_plan.mule_accounts

    for src in sources:
        if len(out) >= budget:
            break
        if rng.coin(0.55):
            ts = random_ts(
                rng,
                base=base,
                day_hi_exclusive=burst_days,
                hour_lo=8,
                hour_hi_exclusive=22,
            )
            if not _append_bounded(
                ctx,
                out,
                budget,
                TxnSpec(
                    src=src,
                    dst=collector,
                    amt=fraud_amount(rng),
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel="fraud_funnel_in",
                ),
            ):
                break

    for _ in range(rng.int(6, 16)):
        if len(out) >= budget:
            break
        dst = rng.choice(cashouts) if cashouts else collector
        ts = random_ts(
            rng,
            base=base,
            day_hi_exclusive=burst_days,
            hour_lo=0,
            hour_hi_exclusive=24,
        )
        if not _append_bounded(
            ctx,
            out,
            budget,
            TxnSpec(
                src=collector,
                dst=dst,
                amt=cycle_amount(rng),
                ts=ts,
                is_fraud=1,
                ring_id=ring_plan.ring_id,
                channel="fraud_funnel_out",
            ),
        ):
            break

    return out


def _inject_structuring(
    ctx: IllicitContext,
    ring_plan: RingPlan,
    budget: int,
) -> list[Txn]:
    if budget <= 0:
        return []

    rng = ctx.execution.rng
    base, burst_days = sample_burst_window(
        rng,
        start_date=ctx.window.start_date,
        days=ctx.window.days,
        base_tail_days=10,
        burst_min=3,
        burst_max_exclusive=8,
    )

    if not ring_plan.mule_accounts and not ring_plan.fraud_accounts:
        return []

    target = (
        rng.choice(ring_plan.mule_accounts)
        if ring_plan.mule_accounts
        else rng.choice(ring_plan.fraud_accounts)
    )

    policy = ctx.structuring_policy
    threshold = float(policy.threshold)
    eps_min = float(policy.epsilon_min)
    eps_max = float(policy.epsilon_max)

    out: list[Txn] = []

    for victim_acct in ring_plan.victim_accounts:
        if len(out) >= budget:
            break
        if not rng.coin(0.55):
            continue

        splits = rng.int(int(policy.splits_min), int(policy.splits_max) + 1)
        for _ in range(splits):
            if len(out) >= budget:
                break
            eps = eps_min + (eps_max - eps_min) * rng.float()
            amt = max(50.0, threshold - eps)
            ts = random_ts(
                rng,
                base=base,
                day_hi_exclusive=burst_days,
                hour_lo=8,
                hour_hi_exclusive=22,
            )
            if not _append_bounded(
                ctx,
                out,
                budget,
                TxnSpec(
                    src=victim_acct,
                    dst=target,
                    amt=amt,
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_plan.ring_id,
                    channel="fraud_structuring",
                ),
            ):
                break

    return out


def _inject_invoice_like(
    ctx: IllicitContext,
    ring_plan: RingPlan,
    budget: int,
) -> list[Txn]:
    if budget <= 0:
        return []
    if not ring_plan.fraud_accounts or not ctx.biller_accounts:
        return []

    rng = ctx.execution.rng
    base = ctx.window.start_date + timedelta(
        days=rng.int(0, max(1, ctx.window.days - 14))
    )
    weeks = max(1, min(6, ctx.window.days // 7))

    out: list[Txn] = []
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
        if not _append_bounded(
            ctx,
            out,
            budget,
            TxnSpec(
                src=src,
                dst=dst,
                amt=amt,
                ts=ts,
                is_fraud=1,
                ring_id=ring_plan.ring_id,
                channel="fraud_invoice",
            ),
        ):
            break

    return out
