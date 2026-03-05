from dataclasses import dataclass
from datetime import datetime, timedelta

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


type Typology = str


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
    ring: RingPeople, accounts: AccountsData
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


def _inject_classic(
    txf: TxnFactory,
    *,
    ring_id: int,
    start_date: datetime,
    days: int,
    fraud_accts: list[str],
    mule_accts: list[str],
    victim_accts: list[str],
) -> list[Txn]:
    rng = txf.rng
    out: list[Txn] = []
    base = start_date + timedelta(days=rng.int(0, max(1, days - 7)))
    burst_days = rng.int(2, 6)

    for va in victim_accts:
        if mule_accts and rng.coin(0.75):
            mule = rng.choice(mule_accts)
            ts = base + timedelta(
                days=rng.int(0, burst_days),
                hours=rng.int(8, 22),
                minutes=rng.int(0, 60),
            )
            out.append(
                txf.make(
                    TxnSpec(
                        src=va,
                        dst=mule,
                        amt=fraud_amount(rng),
                        ts=ts,
                        is_fraud=1,
                        ring_id=ring_id,
                        channel="fraud_classic",
                    )
                )
            )

    for ma in mule_accts:
        if not fraud_accts:
            continue
        forwards = rng.int(2, 6)
        span = min(3, burst_days)
        for _ in range(forwards):
            fa = rng.choice(fraud_accts)
            ts = base + timedelta(
                days=rng.int(0, max(1, span)),
                hours=rng.int(0, 24),
                minutes=rng.int(0, 60),
            )
            out.append(
                txf.make(
                    TxnSpec(
                        src=ma,
                        dst=fa,
                        amt=fraud_amount(rng),
                        ts=ts,
                        is_fraud=1,
                        ring_id=ring_id,
                        channel="fraud_classic",
                    )
                )
            )

    nodes = fraud_accts + mule_accts
    if len(nodes) >= 3:
        k = min(len(nodes), rng.int(3, 7))
        cycle_nodes = rng.choice_k(nodes, k, replace=False)
        passes = rng.int(2, 5)

        for _ in range(passes):
            for i, src in enumerate(cycle_nodes):
                dst = cycle_nodes[(i + 1) % len(cycle_nodes)]
                ts = base + timedelta(
                    days=rng.int(0, burst_days),
                    hours=rng.int(0, 24),
                    minutes=rng.int(0, 60),
                )
                out.append(
                    txf.make(
                        TxnSpec(
                            src=src,
                            dst=dst,
                            amt=cycle_amount(rng),
                            ts=ts,
                            is_fraud=1,
                            ring_id=ring_id,
                            channel="fraud_cycle",
                        )
                    )
                )
    return out


def _inject_layering_chain(
    knobs: FraudInjectionKnobs,
    txf: TxnFactory,
    *,
    ring_id: int,
    start_date: datetime,
    days: int,
    fraud_accts: list[str],
    mule_accts: list[str],
    victim_accts: list[str],
) -> list[Txn]:
    rng = txf.rng
    base = start_date + timedelta(days=rng.int(0, max(1, days - 10)))
    burst_days = rng.int(3, 7)

    nodes = mule_accts + fraud_accts
    if len(nodes) < 3 or not victim_accts:
        return _inject_classic(
            txf,
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

    for va in victim_accts:
        if rng.coin(0.60):
            ts = base + timedelta(
                days=rng.int(0, burst_days),
                hours=rng.int(8, 22),
                minutes=rng.int(0, 60),
            )
            out.append(
                txf.make(
                    TxnSpec(
                        src=va,
                        dst=entry,
                        amt=fraud_amount(rng),
                        ts=ts,
                        is_fraud=1,
                        ring_id=ring_id,
                        channel="fraud_layering_in",
                    )
                )
            )

    for src, dst in zip(chain[:-1], chain[1:]):
        for _ in range(rng.int(1, 4)):
            ts = base + timedelta(
                days=rng.int(0, burst_days),
                hours=rng.int(0, 24),
                minutes=rng.int(0, 60),
            )
            out.append(
                txf.make(
                    TxnSpec(
                        src=src,
                        dst=dst,
                        amt=cycle_amount(rng),
                        ts=ts,
                        is_fraud=1,
                        ring_id=ring_id,
                        channel="fraud_layering_hop",
                    )
                )
            )

    cash = rng.choice(fraud_accts) if fraud_accts else rng.choice(nodes)
    ts = base + timedelta(
        days=rng.int(0, burst_days), hours=rng.int(0, 24), minutes=rng.int(0, 60)
    )
    out.append(
        txf.make(
            TxnSpec(
                src=exit_,
                dst=cash,
                amt=fraud_amount(rng),
                ts=ts,
                is_fraud=1,
                ring_id=ring_id,
                channel="fraud_layering_out",
            )
        )
    )
    return out


def _inject_funnel(
    txf: TxnFactory,
    *,
    ring_id: int,
    start_date: datetime,
    days: int,
    fraud_accts: list[str],
    mule_accts: list[str],
    victim_accts: list[str],
) -> list[Txn]:
    rng = txf.rng
    base = start_date + timedelta(days=rng.int(0, max(1, days - 10)))
    burst_days = rng.int(2, 6)

    pool = fraud_accts + mule_accts
    if len(pool) < 2:
        return _inject_classic(
            txf,
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
        cashouts = [a for a in cashouts if a != collector]

    out: list[Txn] = []

    sources = victim_accts + mule_accts
    for s in sources:
        if rng.coin(0.55):
            ts = base + timedelta(
                days=rng.int(0, burst_days),
                hours=rng.int(8, 22),
                minutes=rng.int(0, 60),
            )
            out.append(
                txf.make(
                    TxnSpec(
                        src=s,
                        dst=collector,
                        amt=fraud_amount(rng),
                        ts=ts,
                        is_fraud=1,
                        ring_id=ring_id,
                        channel="fraud_funnel_in",
                    )
                )
            )

    for _ in range(rng.int(6, 16)):
        dst = rng.choice(cashouts) if cashouts else collector
        ts = base + timedelta(
            days=rng.int(0, burst_days), hours=rng.int(0, 24), minutes=rng.int(0, 60)
        )
        out.append(
            txf.make(
                TxnSpec(
                    src=collector,
                    dst=dst,
                    amt=cycle_amount(rng),
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_id,
                    channel="fraud_funnel_out",
                )
            )
        )

    return out


def _inject_structuring(
    knobs: FraudInjectionKnobs,
    txf: TxnFactory,
    *,
    ring_id: int,
    start_date: datetime,
    days: int,
    fraud_accts: list[str],
    mule_accts: list[str],
    victim_accts: list[str],
) -> list[Txn]:
    rng = txf.rng
    base = start_date + timedelta(days=rng.int(0, max(1, days - 10)))
    burst_days = rng.int(3, 8)

    if not mule_accts and not fraud_accts:
        return []

    target = rng.choice(mule_accts) if mule_accts else rng.choice(fraud_accts)

    thr = float(knobs.structuring_threshold)
    eps_min = float(knobs.structuring_epsilon_min)
    eps_max = float(knobs.structuring_epsilon_max)

    splits_min = max(1, int(knobs.structuring_splits_min))
    splits_max = max(splits_min, int(knobs.structuring_splits_max))

    out: list[Txn] = []

    for va in victim_accts:
        if not rng.coin(0.55):
            continue
        splits = rng.int(splits_min, splits_max + 1)
        for _ in range(splits):
            eps = eps_min + (eps_max - eps_min) * rng.float()
            amt = max(50.0, thr - eps)
            ts = base + timedelta(
                days=rng.int(0, burst_days),
                hours=rng.int(8, 22),
                minutes=rng.int(0, 60),
            )
            out.append(
                txf.make(
                    TxnSpec(
                        src=va,
                        dst=target,
                        amt=amt,
                        ts=ts,
                        is_fraud=1,
                        ring_id=ring_id,
                        channel="fraud_structuring",
                    )
                )
            )

    return out


def _inject_invoice_like(
    txf: TxnFactory,
    *,
    ring_id: int,
    start_date: datetime,
    days: int,
    fraud_accts: list[str],
    biller_accounts: list[str],
) -> list[Txn]:
    rng = txf.rng
    if not fraud_accts or not biller_accounts:
        return []

    base = start_date + timedelta(days=rng.int(0, max(1, days - 14)))
    weeks = max(1, min(6, days // 7))

    out: list[Txn] = []
    for _ in range(rng.int(3, 10)):
        src = rng.choice(fraud_accts)
        dst = rng.choice(biller_accounts)

        ln = float(rng.gen.lognormal(mean=8.0, sigma=0.35))
        amt = round(ln / 10.0) * 10.0

        ts = base + timedelta(
            days=7 * rng.int(0, weeks), hours=rng.int(9, 18), minutes=rng.int(0, 60)
        )
        out.append(
            txf.make(
                TxnSpec(
                    src=src,
                    dst=dst,
                    amt=amt,
                    ts=ts,
                    is_fraud=1,
                    ring_id=ring_id,
                    channel="fraud_invoice",
                )
            )
        )

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
        paydays = [d for d in iter_month_starts(start_date, days) if d >= start_date]
        for pd in paydays:
            for a in ring_accounts:
                if rng.coin(p_bill):
                    dst = rng.choice(biller_accounts)
                    ts = pd + timedelta(
                        days=rng.int(0, 5), hours=rng.int(7, 22), minutes=rng.int(0, 60)
                    )
                    out.append(
                        txf.make(
                            TxnSpec(
                                src=a,
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
        for a in ring_accounts:
            if rng.coin(p_small):
                dst = rng.choice(all_accounts)
                if dst == a:
                    continue
                ts = day_start + timedelta(hours=rng.int(0, 24), minutes=rng.int(0, 60))
                out.append(
                    txf.make(
                        TxnSpec(
                            src=a,
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
        for a in ring_accounts:
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
                            dst=a,
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

    injected: list[Txn] = []

    for ring in people.rings:
        ring_id = ring.ring_id
        fraud_accts, mule_accts, victim_accts = _ring_accounts(ring, accounts)

        typ = _choose_typology(knobs, rng)

        match typ:
            case "classic":
                injected.extend(
                    _inject_classic(
                        txf,
                        ring_id=ring_id,
                        start_date=start_date,
                        days=days,
                        fraud_accts=fraud_accts,
                        mule_accts=mule_accts,
                        victim_accts=victim_accts,
                    )
                )
            case "layering":
                injected.extend(
                    _inject_layering_chain(
                        knobs,
                        txf,
                        ring_id=ring_id,
                        start_date=start_date,
                        days=days,
                        fraud_accts=fraud_accts,
                        mule_accts=mule_accts,
                        victim_accts=victim_accts,
                    )
                )
            case "funnel":
                injected.extend(
                    _inject_funnel(
                        txf,
                        ring_id=ring_id,
                        start_date=start_date,
                        days=days,
                        fraud_accts=fraud_accts,
                        mule_accts=mule_accts,
                        victim_accts=victim_accts,
                    )
                )
            case "structuring":
                injected.extend(
                    _inject_structuring(
                        knobs,
                        txf,
                        ring_id=ring_id,
                        start_date=start_date,
                        days=days,
                        fraud_accts=fraud_accts,
                        mule_accts=mule_accts,
                        victim_accts=victim_accts,
                    )
                )
            case "invoice":
                injected.extend(
                    _inject_invoice_like(
                        txf,
                        ring_id=ring_id,
                        start_date=start_date,
                        days=days,
                        fraud_accts=fraud_accts,
                        biller_accounts=billers,
                    )
                )
            case _:
                injected.extend(
                    _inject_classic(
                        txf,
                        ring_id=ring_id,
                        start_date=start_date,
                        days=days,
                        fraud_accts=fraud_accts,
                        mule_accts=mule_accts,
                        victim_accts=victim_accts,
                    )
                )

        ring_accounts = fraud_accts + mule_accts
        injected.extend(
            _inject_camouflage(
                knobs,
                txf,
                start_date=start_date,
                days=days,
                ring_id=ring_id,
                ring_accounts=ring_accounts,
                all_accounts=accounts.accounts,
                biller_accounts=billers,
                employers=emps,
            )
        )

    out = list(base_txns)
    out.extend(injected)
    return FraudInjectionResult(txns=out, injected_count=len(injected))
