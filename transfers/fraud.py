from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import Literal

from common.config import FraudConfig, WindowConfig
from common.rng import Rng
from common.temporal import iter_month_starts
from common.types import Txn
from entities.accounts import AccountsData
from entities.people import PeopleData, RingPeople
from infra.txn_infra import TxnInfraAssigner
from math_models.amounts import (
    bill_amount,
    cycle_amount,
    fraud_amount,
    p2p_amount,
    salary_amount,
)
from transfers.txns import TxnFactory, TxnSpec


type Typology = Literal["classic", "layering", "funnel", "structuring", "invoice"]


@dataclass(frozen=True, slots=True)
class FraudInjectionKnobs:
    # --- typology mixture weights ---
    fraud_typology_classic_w: float = 0.45
    fraud_typology_layering_w: float = 0.20
    fraud_typology_funnel_w: float = 0.15
    fraud_typology_structuring_w: float = 0.15
    fraud_typology_invoice_w: float = 0.05

    # --- layering ---
    layering_min_hops: int = 3
    layering_max_hops: int = 8

    # --- structuring ---
    structuring_threshold: float = 10_000.0
    structuring_epsilon_min: float = 50.0
    structuring_epsilon_max: float = 400.0
    structuring_splits_min: int = 3
    structuring_splits_max: int = 12

    # --- camouflage ---
    camouflage_small_p2p_per_day_p: float = 0.03
    camouflage_bill_monthly_p: float = 0.35
    camouflage_salary_inbound_p: float = 0.12

    def validate(self) -> None:
        ws = (
            self.fraud_typology_classic_w,
            self.fraud_typology_layering_w,
            self.fraud_typology_funnel_w,
            self.fraud_typology_structuring_w,
            self.fraud_typology_invoice_w,
        )
        if any(w < 0.0 for w in ws):
            raise ValueError("fraud typology weights must be >= 0")

        if self.layering_min_hops < 1:
            raise ValueError("layering_min_hops must be >= 1")
        if self.layering_max_hops < self.layering_min_hops:
            raise ValueError("layering_max_hops must be >= layering_min_hops")

        if self.structuring_threshold <= 0.0:
            raise ValueError("structuring_threshold must be > 0")
        if self.structuring_epsilon_max < self.structuring_epsilon_min:
            raise ValueError(
                "structuring_epsilon_max must be >= structuring_epsilon_min"
            )
        if self.structuring_splits_min < 1:
            raise ValueError("structuring_splits_min must be >= 1")
        if self.structuring_splits_max < self.structuring_splits_min:
            raise ValueError("structuring_splits_max must be >= structuring_splits_min")

        p_small = float(self.camouflage_small_p2p_per_day_p)
        p_bill = float(self.camouflage_bill_monthly_p)
        p_salary = float(self.camouflage_salary_inbound_p)

        if not (0.0 <= p_small <= 1.0):
            raise ValueError(
                "camouflage_small_p2p_per_day_p must be between 0.0 and 1.0"
            )
        if not (0.0 <= p_bill <= 1.0):
            raise ValueError("camouflage_bill_monthly_p must be between 0.0 and 1.0")
        if not (0.0 <= p_salary <= 1.0):
            raise ValueError("camouflage_salary_inbound_p must be between 0.0 and 1.0")


@dataclass(frozen=True, slots=True)
class FraudInjectionResult:
    txns: list[Txn]
    injected_count: int


def _primary_account(accounts: AccountsData, person_id: str) -> str:
    accts = accounts.person_accounts.get(person_id)
    if not accts:
        raise ValueError(f"person has no accounts: {person_id}")
    return accts[0]


def _ring_accounts(
    ring: RingPeople,
    accounts: AccountsData,
) -> tuple[list[str], list[str], list[str]]:
    fraud_accts = [_primary_account(accounts, p) for p in ring.fraud_people]
    mule_accts = [_primary_account(accounts, p) for p in ring.mule_people]
    victim_accts = [_primary_account(accounts, p) for p in ring.victim_people]
    return fraud_accts, mule_accts, victim_accts


def _choose_typology(knobs: FraudInjectionKnobs, rng: Rng) -> Typology:
    items: tuple[Typology, ...] = (
        "classic",
        "layering",
        "funnel",
        "structuring",
        "invoice",
    )
    weights = (
        float(knobs.fraud_typology_classic_w),
        float(knobs.fraud_typology_layering_w),
        float(knobs.fraud_typology_funnel_w),
        float(knobs.fraud_typology_structuring_w),
        float(knobs.fraud_typology_invoice_w),
    )
    if float(sum(weights)) <= 0.0:
        return "classic"
    return rng.weighted_choice(items, weights)


def _append_txn(
    out: list[Txn],
    txf: TxnFactory,
    *,
    budget: int,
    src: str,
    dst: str,
    amt: float,
    ts: datetime,
    ring_id: int,
    channel: str,
    is_fraud: int,
) -> bool:
    if len(out) >= budget:
        return False

    out.append(
        txf.make(
            TxnSpec(
                src=src,
                dst=dst,
                amt=amt,
                ts=ts,
                is_fraud=is_fraud,
                ring_id=ring_id,
                channel=channel,
            )
        )
    )
    return True


def _target_illicit_count(
    *,
    target_ratio: float,
    legit_count: int,
) -> int:
    """
    Solve for illicit count in:
        illicit / (legit + illicit) = target_ratio

    => illicit = target_ratio * legit / (1 - target_ratio)
    """
    if target_ratio <= 0.0 or legit_count <= 0:
        return 0

    denom = max(1e-12, 1.0 - float(target_ratio))
    return max(0, int(round((float(target_ratio) * float(legit_count)) / denom)))


def _month_starts_in_window(start_date: datetime, days: int) -> list[datetime]:
    return [d for d in iter_month_starts(start_date, days) if d >= start_date]


def _sample_burst_window(
    rng: Rng,
    *,
    start_date: datetime,
    days: int,
    base_tail_days: int,
    burst_min: int,
    burst_max_exclusive: int,
) -> tuple[datetime, int]:
    base = start_date + timedelta(days=rng.int(0, max(1, days - base_tail_days)))
    burst_days = rng.int(burst_min, burst_max_exclusive)
    return base, burst_days


def _random_ts(
    rng: Rng,
    *,
    base: datetime,
    day_hi_exclusive: int,
    hour_lo: int,
    hour_hi_exclusive: int,
) -> datetime:
    return base + timedelta(
        days=rng.int(0, max(1, day_hi_exclusive)),
        hours=rng.int(hour_lo, hour_hi_exclusive),
        minutes=rng.int(0, 60),
    )


def _inject_classic(
    txf: TxnFactory,
    *,
    budget: int,
    ring_id: int,
    start_date: datetime,
    days: int,
    fraud_accts: list[str],
    mule_accts: list[str],
    victim_accts: list[str],
) -> list[Txn]:
    if budget <= 0:
        return []

    rng = txf.rng
    out: list[Txn] = []
    base, burst_days = _sample_burst_window(
        rng,
        start_date=start_date,
        days=days,
        base_tail_days=7,
        burst_min=2,
        burst_max_exclusive=6,
    )

    for victim_acct in victim_accts:
        if len(out) >= budget:
            break
        if mule_accts and rng.coin(0.75):
            mule = rng.choice(mule_accts)
            ts = _random_ts(
                rng,
                base=base,
                day_hi_exclusive=burst_days,
                hour_lo=8,
                hour_hi_exclusive=22,
            )
            if not _append_txn(
                out,
                txf,
                budget=budget,
                src=victim_acct,
                dst=mule,
                amt=fraud_amount(rng),
                ts=ts,
                ring_id=ring_id,
                channel="fraud_classic",
                is_fraud=1,
            ):
                break

    for mule_acct in mule_accts:
        if len(out) >= budget:
            break
        if not fraud_accts:
            continue

        forwards = rng.int(2, 6)
        span = min(3, burst_days)

        for _ in range(forwards):
            if len(out) >= budget:
                break
            fraud_acct = rng.choice(fraud_accts)
            ts = _random_ts(
                rng,
                base=base,
                day_hi_exclusive=span,
                hour_lo=0,
                hour_hi_exclusive=24,
            )
            if not _append_txn(
                out,
                txf,
                budget=budget,
                src=mule_acct,
                dst=fraud_acct,
                amt=fraud_amount(rng),
                ts=ts,
                ring_id=ring_id,
                channel="fraud_classic",
                is_fraud=1,
            ):
                break

    nodes = fraud_accts + mule_accts
    if len(nodes) >= 3 and len(out) < budget:
        k = min(len(nodes), rng.int(3, 7))
        cycle_nodes = rng.choice_k(nodes, k, replace=False)
        passes = rng.int(2, 5)

        for _ in range(passes):
            if len(out) >= budget:
                break
            for i, src in enumerate(cycle_nodes):
                if len(out) >= budget:
                    break
                dst = cycle_nodes[(i + 1) % len(cycle_nodes)]
                ts = _random_ts(
                    rng,
                    base=base,
                    day_hi_exclusive=burst_days,
                    hour_lo=0,
                    hour_hi_exclusive=24,
                )
                if not _append_txn(
                    out,
                    txf,
                    budget=budget,
                    src=src,
                    dst=dst,
                    amt=cycle_amount(rng),
                    ts=ts,
                    ring_id=ring_id,
                    channel="fraud_cycle",
                    is_fraud=1,
                ):
                    break

    return out


def _inject_layering_chain(
    knobs: FraudInjectionKnobs,
    txf: TxnFactory,
    *,
    budget: int,
    ring_id: int,
    start_date: datetime,
    days: int,
    fraud_accts: list[str],
    mule_accts: list[str],
    victim_accts: list[str],
) -> list[Txn]:
    if budget <= 0:
        return []

    rng = txf.rng
    base, burst_days = _sample_burst_window(
        rng,
        start_date=start_date,
        days=days,
        base_tail_days=10,
        burst_min=3,
        burst_max_exclusive=7,
    )

    nodes = mule_accts + fraud_accts
    if len(nodes) < 3 or not victim_accts:
        return _inject_classic(
            txf,
            budget=budget,
            ring_id=ring_id,
            start_date=start_date,
            days=days,
            fraud_accts=fraud_accts,
            mule_accts=mule_accts,
            victim_accts=victim_accts,
        )

    hops_min = max(3, int(knobs.layering_min_hops))
    hops_max = max(hops_min, int(knobs.layering_max_hops))
    hops = rng.int(hops_min, hops_max + 1)

    chain = rng.choice_k(nodes, min(hops, len(nodes)), replace=False)
    while len(chain) < hops:
        chain.append(rng.choice(nodes))

    entry = chain[0]
    exit_ = chain[-1]

    out: list[Txn] = []

    for victim_acct in victim_accts:
        if len(out) >= budget:
            break
        if rng.coin(0.60):
            ts = _random_ts(
                rng,
                base=base,
                day_hi_exclusive=burst_days,
                hour_lo=8,
                hour_hi_exclusive=22,
            )
            if not _append_txn(
                out,
                txf,
                budget=budget,
                src=victim_acct,
                dst=entry,
                amt=fraud_amount(rng),
                ts=ts,
                ring_id=ring_id,
                channel="fraud_layering_in",
                is_fraud=1,
            ):
                break

    for src, dst in zip(chain[:-1], chain[1:]):
        if len(out) >= budget:
            break

        for _ in range(rng.int(1, 4)):
            if len(out) >= budget:
                break
            ts = _random_ts(
                rng,
                base=base,
                day_hi_exclusive=burst_days,
                hour_lo=0,
                hour_hi_exclusive=24,
            )
            if not _append_txn(
                out,
                txf,
                budget=budget,
                src=src,
                dst=dst,
                amt=cycle_amount(rng),
                ts=ts,
                ring_id=ring_id,
                channel="fraud_layering_hop",
                is_fraud=1,
            ):
                break

    if len(out) < budget:
        cash = rng.choice(fraud_accts) if fraud_accts else rng.choice(nodes)
        ts = _random_ts(
            rng,
            base=base,
            day_hi_exclusive=burst_days,
            hour_lo=0,
            hour_hi_exclusive=24,
        )
        _ = _append_txn(
            out,
            txf,
            budget=budget,
            src=exit_,
            dst=cash,
            amt=fraud_amount(rng),
            ts=ts,
            ring_id=ring_id,
            channel="fraud_layering_out",
            is_fraud=1,
        )

    return out


def _inject_funnel(
    txf: TxnFactory,
    *,
    budget: int,
    ring_id: int,
    start_date: datetime,
    days: int,
    fraud_accts: list[str],
    mule_accts: list[str],
    victim_accts: list[str],
) -> list[Txn]:
    if budget <= 0:
        return []

    rng = txf.rng
    base, burst_days = _sample_burst_window(
        rng,
        start_date=start_date,
        days=days,
        base_tail_days=10,
        burst_min=2,
        burst_max_exclusive=6,
    )

    pool = fraud_accts + mule_accts
    if len(pool) < 2:
        return _inject_classic(
            txf,
            budget=budget,
            ring_id=ring_id,
            start_date=start_date,
            days=days,
            fraud_accts=fraud_accts,
            mule_accts=mule_accts,
            victim_accts=victim_accts,
        )

    collector = rng.choice(pool)
    cashouts = rng.choice_k(pool, min(4, len(pool)), replace=False)
    if collector in cashouts and len(cashouts) > 1:
        cashouts = [acct for acct in cashouts if acct != collector]

    out: list[Txn] = []
    sources = victim_accts + mule_accts

    for src in sources:
        if len(out) >= budget:
            break
        if rng.coin(0.55):
            ts = _random_ts(
                rng,
                base=base,
                day_hi_exclusive=burst_days,
                hour_lo=8,
                hour_hi_exclusive=22,
            )
            if not _append_txn(
                out,
                txf,
                budget=budget,
                src=src,
                dst=collector,
                amt=fraud_amount(rng),
                ts=ts,
                ring_id=ring_id,
                channel="fraud_funnel_in",
                is_fraud=1,
            ):
                break

    for _ in range(rng.int(6, 16)):
        if len(out) >= budget:
            break
        dst = rng.choice(cashouts) if cashouts else collector
        ts = _random_ts(
            rng,
            base=base,
            day_hi_exclusive=burst_days,
            hour_lo=0,
            hour_hi_exclusive=24,
        )
        if not _append_txn(
            out,
            txf,
            budget=budget,
            src=collector,
            dst=dst,
            amt=cycle_amount(rng),
            ts=ts,
            ring_id=ring_id,
            channel="fraud_funnel_out",
            is_fraud=1,
        ):
            break

    return out


def _inject_structuring(
    knobs: FraudInjectionKnobs,
    txf: TxnFactory,
    *,
    budget: int,
    ring_id: int,
    start_date: datetime,
    days: int,
    fraud_accts: list[str],
    mule_accts: list[str],
    victim_accts: list[str],
) -> list[Txn]:
    if budget <= 0:
        return []

    rng = txf.rng
    base, burst_days = _sample_burst_window(
        rng,
        start_date=start_date,
        days=days,
        base_tail_days=10,
        burst_min=3,
        burst_max_exclusive=8,
    )

    if not mule_accts and not fraud_accts:
        return []

    target = rng.choice(mule_accts) if mule_accts else rng.choice(fraud_accts)

    threshold = float(knobs.structuring_threshold)
    eps_min = float(knobs.structuring_epsilon_min)
    eps_max = float(knobs.structuring_epsilon_max)

    splits_min = max(1, int(knobs.structuring_splits_min))
    splits_max = max(splits_min, int(knobs.structuring_splits_max))

    out: list[Txn] = []

    for victim_acct in victim_accts:
        if len(out) >= budget:
            break
        if not rng.coin(0.55):
            continue

        splits = rng.int(splits_min, splits_max + 1)
        for _ in range(splits):
            if len(out) >= budget:
                break
            eps = eps_min + (eps_max - eps_min) * rng.float()
            amt = max(50.0, threshold - eps)
            ts = _random_ts(
                rng,
                base=base,
                day_hi_exclusive=burst_days,
                hour_lo=8,
                hour_hi_exclusive=22,
            )
            if not _append_txn(
                out,
                txf,
                budget=budget,
                src=victim_acct,
                dst=target,
                amt=amt,
                ts=ts,
                ring_id=ring_id,
                channel="fraud_structuring",
                is_fraud=1,
            ):
                break

    return out


def _inject_invoice_like(
    txf: TxnFactory,
    *,
    budget: int,
    ring_id: int,
    start_date: datetime,
    days: int,
    fraud_accts: list[str],
    biller_accounts: list[str],
) -> list[Txn]:
    if budget <= 0:
        return []

    rng = txf.rng
    if not fraud_accts or not biller_accounts:
        return []

    base = start_date + timedelta(days=rng.int(0, max(1, days - 14)))
    weeks = max(1, min(6, days // 7))

    out: list[Txn] = []
    for _ in range(rng.int(3, 10)):
        if len(out) >= budget:
            break

        src = rng.choice(fraud_accts)
        dst = rng.choice(biller_accounts)

        ln = float(rng.gen.lognormal(mean=8.0, sigma=0.35))
        amt = round(ln / 10.0) * 10.0

        ts = base + timedelta(
            days=7 * rng.int(0, weeks),
            hours=rng.int(9, 18),
            minutes=rng.int(0, 60),
        )
        if not _append_txn(
            out,
            txf,
            budget=budget,
            src=src,
            dst=dst,
            amt=amt,
            ts=ts,
            ring_id=ring_id,
            channel="fraud_invoice",
            is_fraud=1,
        ):
            break

    return out


def _inject_camouflage(
    knobs: FraudInjectionKnobs,
    txf: TxnFactory,
    *,
    start_date: datetime,
    days: int,
    ring_id: int,
    ring_accounts: list[str],
    all_accounts: list[str],
    biller_accounts: list[str],
    employers: list[str],
) -> list[Txn]:
    rng = txf.rng
    if not ring_accounts:
        return []

    p_small = float(knobs.camouflage_small_p2p_per_day_p)
    p_bill = float(knobs.camouflage_bill_monthly_p)
    p_salary_in = float(knobs.camouflage_salary_inbound_p)

    out: list[Txn] = []

    if biller_accounts and p_bill > 0.0:
        for pay_day in _month_starts_in_window(start_date, days):
            for acct in ring_accounts:
                if rng.coin(p_bill):
                    dst = rng.choice(biller_accounts)
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
                                ring_id=ring_id,
                                channel="camouflage_bill",
                            )
                        )
                    )

    for day in range(days):
        day_start = start_date + timedelta(days=day)
        for acct in ring_accounts:
            if rng.coin(p_small):
                dst = rng.choice(all_accounts)
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
                            ring_id=ring_id,
                            channel="camouflage_p2p",
                        )
                    )
                )

    if employers and p_salary_in > 0.0:
        for acct in ring_accounts:
            if rng.coin(p_salary_in):
                src = rng.choice(employers)
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
                            ring_id=ring_id,
                            channel="camouflage_salary",
                        )
                    )
                )

    return out


def inject_fraud_transfers(
    fraud_cfg: FraudConfig,
    window: WindowConfig,
    knobs: FraudInjectionKnobs,
    rng: Rng,
    people: PeopleData,
    accounts: AccountsData,
    base_txns: list[Txn],
    *,
    biller_accounts: list[str] | None = None,
    employers: list[str] | None = None,
    infra: TxnInfraAssigner | None = None,
) -> FraudInjectionResult:
    knobs.validate()

    start_date = window.start_date()
    days = int(window.days)

    if fraud_cfg.fraud_rings <= 0 or not people.rings:
        return FraudInjectionResult(txns=list(base_txns), injected_count=0)

    billers = biller_accounts or []
    emps = employers or []
    txf = TxnFactory(rng=rng, infra=infra)

    camouflage: list[Txn] = []
    ring_acct_cache: dict[int, tuple[list[str], list[str], list[str]]] = {}

    for ring in people.rings:
        fraud_accts, mule_accts, victim_accts = _ring_accounts(ring, accounts)
        ring_acct_cache[ring.ring_id] = (fraud_accts, mule_accts, victim_accts)

        ring_accounts = fraud_accts + mule_accts
        camouflage.extend(
            _inject_camouflage(
                knobs,
                txf,
                start_date=start_date,
                days=days,
                ring_id=ring.ring_id,
                ring_accounts=ring_accounts,
                all_accounts=accounts.accounts,
                biller_accounts=billers,
                employers=emps,
            )
        )

    target_ratio = float(fraud_cfg.target_illicit_ratio)
    legit_denominator = len(base_txns) + len(camouflage)
    target_illicit_n = _target_illicit_count(
        target_ratio=target_ratio,
        legit_count=legit_denominator,
    )

    if target_illicit_n <= 0:
        out = list(base_txns)
        out.extend(camouflage)
        return FraudInjectionResult(txns=out, injected_count=len(camouflage))

    illicit: list[Txn] = []
    remaining_budget = target_illicit_n

    for i, ring in enumerate(people.rings):
        if remaining_budget <= 0:
            break

        fraud_accts, mule_accts, victim_accts = ring_acct_cache[ring.ring_id]

        rings_left = len(people.rings) - i
        ring_budget = max(1, remaining_budget // max(1, rings_left))
        ring_budget = min(ring_budget, remaining_budget)

        typ = _choose_typology(knobs, rng)

        match typ:
            case "classic":
                ring_illicit = _inject_classic(
                    txf,
                    budget=ring_budget,
                    ring_id=ring.ring_id,
                    start_date=start_date,
                    days=days,
                    fraud_accts=fraud_accts,
                    mule_accts=mule_accts,
                    victim_accts=victim_accts,
                )
            case "layering":
                ring_illicit = _inject_layering_chain(
                    knobs,
                    txf,
                    budget=ring_budget,
                    ring_id=ring.ring_id,
                    start_date=start_date,
                    days=days,
                    fraud_accts=fraud_accts,
                    mule_accts=mule_accts,
                    victim_accts=victim_accts,
                )
            case "funnel":
                ring_illicit = _inject_funnel(
                    txf,
                    budget=ring_budget,
                    ring_id=ring.ring_id,
                    start_date=start_date,
                    days=days,
                    fraud_accts=fraud_accts,
                    mule_accts=mule_accts,
                    victim_accts=victim_accts,
                )
            case "structuring":
                ring_illicit = _inject_structuring(
                    knobs,
                    txf,
                    budget=ring_budget,
                    ring_id=ring.ring_id,
                    start_date=start_date,
                    days=days,
                    fraud_accts=fraud_accts,
                    mule_accts=mule_accts,
                    victim_accts=victim_accts,
                )
            case "invoice":
                ring_illicit = _inject_invoice_like(
                    txf,
                    budget=ring_budget,
                    ring_id=ring.ring_id,
                    start_date=start_date,
                    days=days,
                    fraud_accts=fraud_accts,
                    biller_accounts=billers,
                )

        illicit.extend(ring_illicit)
        remaining_budget -= len(ring_illicit)

    out = list(base_txns)
    out.extend(camouflage)
    out.extend(illicit)

    return FraudInjectionResult(
        txns=out,
        injected_count=len(camouflage) + len(illicit),
    )
