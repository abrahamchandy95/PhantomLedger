from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

import numpy as np

from common.config import EventsConfig
from common.rng import Rng
from common.types import Txn
from entities.personas import PERSONAS
from infra.txn_infra import TxnInfraAssigner
from math_models.graph import NeighborGraph
from math_models.timing import (
    TimingProfiles,
    default_timing_profiles,
    sample_offsets_seconds,
)
from math_models.amounts import p2p_amount
from math_models.counts import weekday_multiplier, DEFAULT_COUNT_MODELS


type _NumScalar = float | int | np.floating | np.integer


def _as_float(x: object) -> float:
    return float(cast(_NumScalar, x))


def _as_int(x: object) -> int:
    return int(cast(int | np.integer, x))


@dataclass(frozen=True, slots=True)
class DayToDayContext:
    accounts: list[str]
    acct_index: dict[str, int]
    hub_set_idx: set[int]
    biller_set_idx: set[int]
    clearing_set_idx: set[int]  # subset of billers/hubs used as "unknown outflow" sinks
    persona_for_acct: np.ndarray  # (n_accounts,) dtype=int8
    persona_names: list[str]
    timing: TimingProfiles


def _supports_txn_fields() -> tuple[bool, bool, bool]:
    """
    Returns (supports_device_id, supports_ip_address, supports_channel).
    Typed so kwargs can be built without using object-typed values.
    """
    fields_obj: object = getattr(Txn, "__dataclass_fields__", {})
    if isinstance(fields_obj, dict):
        fields = cast(dict[str, object], fields_obj)
        return ("device_id" in fields, "ip_address" in fields, "channel" in fields)
    return (False, False, False)


_SUPPORTS_DEVICE, _SUPPORTS_IP, _SUPPORTS_CHANNEL = _supports_txn_fields()


def _sample_day_multiplier(ecfg: EventsConfig, rng: Rng) -> float:
    k = float(ecfg.day_multiplier_gamma_shape)
    if k <= 0.0:
        raise ValueError("day_multiplier_gamma_shape must be > 0")
    g_obj: object = cast(object, rng.gen.gamma(shape=k, scale=1.0 / k))
    return _as_float(g_obj)


def _build_src_probs(base_rates: np.ndarray) -> np.ndarray:
    s = _as_float(np.sum(base_rates, dtype=np.float64))
    if s <= 0.0 or not np.isfinite(s):
        raise ValueError("sum of rates must be > 0 and finite")
    return (base_rates / s).astype(np.float64)


def _sample_dst_indices(
    rng: Rng, src_idx: np.ndarray, graph: NeighborGraph
) -> np.ndarray:
    n_events = int(src_idx.size)
    if n_events == 0:
        return np.zeros(0, dtype=np.int32)

    u_obj: object = cast(object, rng.gen.random(size=n_events))
    u = np.asarray(u_obj, dtype=np.float32)

    cdf_rows_obj: object = cast(object, graph.cdf[src_idx])
    cdf_rows = np.asarray(cdf_rows_obj, dtype=np.float32)

    cmp_obj: object = cast(object, cdf_rows < u[:, None])
    cmp = np.asarray(cmp_obj, dtype=np.bool_)

    j_obj: object = cast(object, np.count_nonzero(cmp, axis=1))
    j = np.asarray(j_obj, dtype=np.int32)

    dst_obj: object = cast(object, graph.neighbors[src_idx, j])
    return np.asarray(dst_obj, dtype=np.int32)


def _make_txn(
    src: str,
    dst: str,
    amt: float,
    ts: datetime,
    *,
    is_fraud: int,
    ring_id: int,
    device_id: str | None,
    ip_address: str | None,
    channel: str | None,
) -> Txn:
    """
    Construct a Txn while only passing optional fields if the Txn type supports them.
    Avoids passing object-typed kwargs (which triggers basedpyright errors).
    """
    if _SUPPORTS_DEVICE or _SUPPORTS_IP or _SUPPORTS_CHANNEL:
        if _SUPPORTS_DEVICE and _SUPPORTS_IP and _SUPPORTS_CHANNEL:
            return Txn(
                src, dst, amt, ts, is_fraud, ring_id, device_id, ip_address, channel
            )
        if _SUPPORTS_DEVICE and _SUPPORTS_IP:
            return Txn(src, dst, amt, ts, is_fraud, ring_id, device_id, ip_address)
        if _SUPPORTS_DEVICE and _SUPPORTS_CHANNEL:
            return Txn(src, dst, amt, ts, is_fraud, ring_id, device_id, None, channel)
        if _SUPPORTS_IP and _SUPPORTS_CHANNEL:
            return Txn(src, dst, amt, ts, is_fraud, ring_id, None, ip_address, channel)
        if _SUPPORTS_DEVICE:
            return Txn(src, dst, amt, ts, is_fraud, ring_id, device_id)
        if _SUPPORTS_IP:
            return Txn(src, dst, amt, ts, is_fraud, ring_id, None, ip_address)
        if _SUPPORTS_CHANNEL:
            return Txn(src, dst, amt, ts, is_fraud, ring_id, None, None, channel)

    return Txn(src, dst, amt, ts, is_fraud, ring_id)


def generate_day_to_day_superposition(
    ecfg: EventsConfig,
    rng: Rng,
    *,
    start_date: datetime,
    days: int,
    ctx: DayToDayContext,
    graph: NeighborGraph,
    infra: TxnInfraAssigner | None = None,
    base_mu: float = -2.0,
    base_sigma: float = 1.0,
    hub_activity_boost: float = 30.0,
) -> list[Txn]:
    n = len(ctx.accounts)
    if n == 0 or days <= 0:
        return []

    rates_obj: object = cast(
        object, rng.gen.lognormal(mean=base_mu, sigma=base_sigma, size=n)
    )
    rates = np.asarray(rates_obj, dtype=np.float64)

    for i in ctx.hub_set_idx:
        rates[i] *= float(hub_activity_boost)

    persona_arr_obj: object = cast(object, ctx.persona_for_acct)
    persona_arr = np.asarray(persona_arr_obj, dtype=np.int64)

    for pid, pname in enumerate(ctx.persona_names):
        spec = PERSONAS[pname]
        mask_obj: object = cast(object, persona_arr == int(pid))
        mask = np.asarray(mask_obj, dtype=np.bool_)
        if int(np.count_nonzero(mask)) > 0:
            rates[mask] *= float(spec.rate_mult)

    src_probs = _build_src_probs(rates)
    txns: list[Txn] = []

    prefer_billers_p = float(ecfg.prefer_billers_p)
    unknown_outflow_p = float(ecfg.unknown_outflow_p)

    biller_list = np.fromiter(ctx.biller_set_idx, dtype=np.int32)
    has_billers = bool(biller_list.size > 0)

    clearing_list = np.fromiter(ctx.clearing_set_idx, dtype=np.int32)
    has_clearing = bool(clearing_list.size > 0)

    sum_rates = _as_float(np.sum(rates, dtype=np.float64))

    for day in range(days):
        day_start = start_date + timedelta(days=day)
        wd_mult = float(weekday_multiplier(day_start, DEFAULT_COUNT_MODELS))
        m = _sample_day_multiplier(ecfg, rng) * wd_mult

        lam = m * sum_rates
        if lam <= 0.0 or not np.isfinite(lam):
            continue

        pe_obj: object = cast(object, rng.gen.poisson(lam=lam))
        n_events = _as_int(pe_obj)

        cap = int(ecfg.max_events_per_day)
        if cap > 0 and n_events > cap:
            n_events = cap
        if n_events <= 0:
            continue

        src_obj: object = cast(
            object, rng.gen.choice(n, size=n_events, replace=True, p=src_probs)
        )
        src_idx = np.asarray(src_obj, dtype=np.int32)

        dst_idx = _sample_dst_indices(rng, src_idx, graph)

        bill_mask = np.zeros(n_events, dtype=np.bool_)
        out_mask = np.zeros(n_events, dtype=np.bool_)

        if has_billers and prefer_billers_p > 0.0:
            bm_obj: object = cast(object, rng.gen.random(size=n_events))
            bm_u = np.asarray(bm_obj, dtype=np.float32)
            bill_mask = bm_u < float(prefer_billers_p)

            bill_cnt = int(np.count_nonzero(bill_mask))
            if bill_cnt > 0:
                chosen_obj: object = cast(
                    object, rng.gen.choice(biller_list, size=bill_cnt, replace=True)
                )
                chosen = np.asarray(chosen_obj, dtype=np.int32)
                dst_idx[bill_mask] = chosen

        if has_clearing and unknown_outflow_p > 0.0:
            uo_obj: object = cast(object, rng.gen.random(size=n_events))
            uo_u = np.asarray(uo_obj, dtype=np.float32)
            out_mask = uo_u < float(unknown_outflow_p)

            out_cnt = int(np.count_nonzero(out_mask))
            if out_cnt > 0:
                chosen_obj2: object = cast(
                    object, rng.gen.choice(clearing_list, size=out_cnt, replace=True)
                )
                chosen2 = np.asarray(chosen_obj2, dtype=np.int32)
                dst_idx[out_mask] = chosen2

        same_obj: object = cast(object, dst_idx == src_idx)
        same = np.asarray(same_obj, dtype=np.bool_)
        if int(np.count_nonzero(same)) > 0:
            dst_idx[same] = (dst_idx[same] + 1) % n

        offsets = np.empty(n_events, dtype=np.int32)

        ev_persona_obj: object = cast(object, persona_arr[src_idx])
        ev_persona = np.asarray(ev_persona_obj, dtype=np.int64)

        for pid, pname in enumerate(ctx.persona_names):
            spec = PERSONAS[pname]
            pm_obj: object = cast(object, ev_persona == int(pid))
            pmask = np.asarray(pm_obj, dtype=np.bool_)
            cnt = int(np.count_nonzero(pmask))
            if cnt <= 0:
                continue
            offsets[pmask] = sample_offsets_seconds(
                rng, spec.timing_profile, cnt, ctx.timing
            )

        for i in range(n_events):
            si_obj: object = cast(object, src_idx[i])
            di_obj: object = cast(object, dst_idx[i])
            oi_obj: object = cast(object, offsets[i])

            si = _as_int(si_obj)
            di = _as_int(di_obj)
            off = _as_int(oi_obj)

            src = ctx.accounts[si]
            dst = ctx.accounts[di]
            ts = day_start + timedelta(seconds=off)

            p_id_obj: object = cast(object, ev_persona[i])
            p_id = _as_int(p_id_obj)

            pname = ctx.persona_names[p_id]
            mult = float(PERSONAS[pname].amount_mult)

            amt_obj: object = cast(object, p2p_amount(rng))
            amt = _as_float(amt_obj) * mult
            amt = round(max(1.0, amt), 2)

            out_i_obj: object = cast(object, out_mask[i])
            bill_i_obj: object = cast(object, bill_mask[i])

            out_i = bool(np.bool_(out_i_obj))
            bill_i = bool(np.bool_(bill_i_obj))

            if out_i:
                channel = "external_unknown"
            elif bill_i:
                channel = "merchant"
            else:
                channel = "p2p"

            device_id: str | None = None
            ip_address: str | None = None
            if infra is not None:
                device_id, ip_address = infra.pick_for_src(rng, src)

            txns.append(
                _make_txn(
                    src,
                    dst,
                    amt,
                    ts,
                    is_fraud=0,
                    ring_id=-1,
                    device_id=device_id,
                    ip_address=ip_address,
                    channel=channel,
                )
            )

    return txns


def build_day_to_day_context(
    ecfg: EventsConfig,
    *,
    accounts: list[str],
    hub_accounts: list[str],
    biller_accounts: list[str],
    persona_for_person: dict[str, str],
    acct_owner: dict[str, str],
    persona_names_override: list[str] | None = None,
) -> DayToDayContext:
    """
    Build the day-to-day context from accounts + persona assignments.

    Depends only on EventsConfig (for clearing_accounts_n) and an optional
    persona_names_override (so we don't need to reach into a global config object).
    """
    acct_index = {a: i for i, a in enumerate(accounts)}

    hub_set_idx = {acct_index[a] for a in hub_accounts if a in acct_index}
    biller_set_idx = {acct_index[a] for a in biller_accounts if a in acct_index}

    clearing_n = int(ecfg.clearing_accounts_n)
    clearing_accounts = biller_accounts[: max(0, min(clearing_n, len(biller_accounts)))]
    clearing_set_idx = {acct_index[a] for a in clearing_accounts if a in acct_index}

    persona_names = persona_names_override or [
        "student",
        "retired",
        "freelancer",
        "smallbiz",
        "hnw",
        "salaried",
    ]

    persona_id_for_name = {n: i for i, n in enumerate(persona_names)}

    persona_for_acct = np.empty(len(accounts), dtype=np.int8)
    for i, a in enumerate(accounts):
        p = acct_owner.get(a)
        pname = persona_for_person.get(p, "salaried") if p is not None else "salaried"
        persona_for_acct[i] = int(
            persona_id_for_name.get(pname, persona_id_for_name["salaried"])
        )

    timing = default_timing_profiles()

    return DayToDayContext(
        accounts=accounts,
        acct_index=acct_index,
        hub_set_idx=hub_set_idx,
        biller_set_idx=biller_set_idx,
        clearing_set_idx=clearing_set_idx,
        persona_for_acct=persona_for_acct,
        persona_names=persona_names,
        timing=timing,
    )
