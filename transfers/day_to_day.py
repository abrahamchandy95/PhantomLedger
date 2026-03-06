from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

import numpy as np
import numpy.typing as npt

from common.config import EventsConfig, MerchantsConfig
from common.rng import Rng
from common.seeding import derived_seed
from common.types import Txn
from entities.merchants import MerchantData
from entities.personas import PERSONAS
from infra.txn_infra import TxnInfraAssigner
from math_models.amounts import bill_amount, p2p_amount
from math_models.counts import (
    DEFAULT_COUNT_MODELS,
    gamma_poisson_out_count,
    weekday_multiplier,
)
from math_models.timing import (
    TimingProfiles,
    default_timing_profiles,
    sample_offsets_seconds,
)
from relationships.social import SocialGraph
from transfers.txns import TxnFactory, TxnSpec

# Python 3.12+ (and therefore 3.14) typing syntax (PEP 695)
type NumScalar = float | int | np.floating | np.integer
type ArrF64 = npt.NDArray[np.float64]
type ArrI32 = npt.NDArray[np.int32]
type ArrI16 = npt.NDArray[np.int16]
type ArrF32 = npt.NDArray[np.float32]
type ArrBool = npt.NDArray[np.bool_]


def _as_float(x: object) -> float:
    return float(cast(NumScalar, x))


def _as_int(x: object) -> int:
    return int(cast(int | np.integer, x))


def _build_cdf(weights: ArrF64) -> ArrF64:
    """
    Normalize weights into a CDF. Last element is exactly 1.0.
    """
    w = np.asarray(weights, dtype=np.float64)

    s_obj: object = cast(object, np.sum(w, dtype=np.float64))
    s = _as_float(s_obj)

    if not np.isfinite(s) or s <= 0.0:
        w[:] = 1.0
        s = float(w.size)

    cdf_obj: object = cast(object, np.cumsum(w / s, dtype=np.float64))
    cdf = np.asarray(cdf_obj, dtype=np.float64)
    cdf[-1] = 1.0
    return cdf


def _cdf_pick_u(cdf: ArrF64, u: float) -> int:
    j_obj: object = cast(object, np.searchsorted(cdf, u, side="right"))
    j = _as_int(j_obj)
    if j >= int(cdf.size):
        j = int(cdf.size) - 1
    return j


def _pick_unique_weighted(gen: np.random.Generator, cdf: ArrF64, k: int) -> list[int]:
    """
    Pick k unique indices using a CDF sampler. k is small (<= ~30).
    """
    out: list[int] = []
    seen: set[int] = set()
    tries = 0
    max_tries = 250

    while len(out) < k and tries < max_tries:
        u = float(gen.random())
        j = _cdf_pick_u(cdf, u)
        tries += 1
        if j in seen:
            continue
        seen.add(j)
        out.append(j)

    if not out:
        out.append(0)
    return out


def _row_contains(arr_row: ArrI32, k: int, v: int) -> bool:
    for i in range(k):
        # Avoid numpy-stub Any leakage from arr_row[i]
        if _as_int(cast(object, arr_row[i])) == v:
            return True
    return False


@dataclass(frozen=True, slots=True)
class DayToDayContext:
    persons: list[str]
    people_index: dict[str, int]

    primary_acct_for_person: dict[str, str]
    persona_for_person: dict[str, str]

    timing: TimingProfiles

    merchants: MerchantData
    merch_cdf: ArrF64

    social: SocialGraph

    fav_merchants_idx: ArrI32  # (n_persons, fav_max)
    fav_k: ArrI16  # (n_persons,)

    billers_idx: ArrI32  # (n_persons, bill_max)
    bill_k: ArrI16  # (n_persons,)

    explore_propensity: ArrF32  # (n_persons,)
    burst_start_day: ArrI32  # (n_persons,)
    burst_len: ArrI16  # (n_persons,)


def build_day_to_day_context(
    ecfg: EventsConfig,
    mcfg: MerchantsConfig,
    rng: Rng,
    *,
    start_date: datetime,
    days: int,
    persons: list[str],
    primary_acct_for_person: dict[str, str],
    persona_for_person: dict[str, str],
    merchants: MerchantData,
    social: SocialGraph,
    base_seed: int,
) -> DayToDayContext:
    # Touch these so strict linters don't complain; no semantic effect.
    _ = (float(ecfg.prefer_billers_p), start_date.year)

    timing = default_timing_profiles()

    merch_w: ArrF64 = np.asarray(merchants.weight, dtype=np.float64)
    merch_cdf: ArrF64 = _build_cdf(merch_w)

    # Build biller-only weights (utilities/telco/insurance/education), fallback to global
    biller_cats: set[str] = {"utilities", "telecom", "insurance", "education"}
    mask_list: list[bool] = [c in biller_cats for c in merchants.category]
    mask: ArrBool = np.asarray(mask_list, dtype=np.bool_)

    biller_w: ArrF64 = merch_w * mask.astype(np.float64)

    biller_sum_obj: object = cast(object, np.sum(biller_w, dtype=np.float64))
    biller_sum = _as_float(biller_sum_obj)
    biller_cdf: ArrF64 = _build_cdf(biller_w) if biller_sum > 0.0 else merch_cdf

    n = len(persons)
    people_index = {p: i for i, p in enumerate(persons)}

    fav_max = int(mcfg.fav_merchants_max)
    bill_max = int(mcfg.billers_max)

    fav_idx: ArrI32 = np.empty((n, fav_max), dtype=np.int32)
    fav_k: ArrI16 = np.empty(n, dtype=np.int16)

    bill_idx: ArrI32 = np.empty((n, bill_max), dtype=np.int32)
    bill_k: ArrI16 = np.empty(n, dtype=np.int16)

    explore_prop: ArrF32 = np.empty(n, dtype=np.float32)
    burst_start: ArrI32 = np.full(n, -1, dtype=np.int32)
    burst_len: ArrI16 = np.zeros(n, dtype=np.int16)

    burst_p = 0.08
    burst_len_min = 3
    burst_len_max = 9

    for i, p in enumerate(persons):
        g = np.random.default_rng(derived_seed(base_seed, "payees", p))

        fk = int(
            g.integers(int(mcfg.fav_merchants_min), int(mcfg.fav_merchants_max) + 1)
        )
        bk = int(g.integers(int(mcfg.billers_min), int(mcfg.billers_max) + 1))

        fav = _pick_unique_weighted(g, merch_cdf, fk)
        bill = _pick_unique_weighted(g, biller_cdf, bk)

        fav_k[i] = fk
        bill_k[i] = bk

        fav_idx[i, :] = int(fav[0])
        fav_idx[i, : len(fav)] = np.asarray(fav, dtype=np.int32)

        bill_idx[i, :] = int(bill[0])
        bill_idx[i, : len(bill)] = np.asarray(bill, dtype=np.int32)

        # Heterogeneous exploration propensity (many low, few higher)
        alpha = 1.6
        beta = 9.5
        explore_prop[i] = np.float32(float(g.beta(alpha, beta)))

        # Optional novelty burst window
        if days > 0 and float(g.random()) < burst_p:
            bs = int(g.integers(0, max(1, days)))
            bl = int(g.integers(burst_len_min, burst_len_max + 1))
            burst_start[i] = bs
            burst_len[i] = np.int16(bl)

    _ = rng.float()

    return DayToDayContext(
        persons=persons,
        people_index=people_index,
        primary_acct_for_person=primary_acct_for_person,
        persona_for_person=persona_for_person,
        timing=timing,
        merchants=merchants,
        merch_cdf=merch_cdf,
        social=social,
        fav_merchants_idx=fav_idx,
        fav_k=fav_k,
        billers_idx=bill_idx,
        bill_k=bill_k,
        explore_propensity=explore_prop,
        burst_start_day=burst_start,
        burst_len=burst_len,
    )


def generate_day_to_day_superposition(
    ecfg: EventsConfig,
    mcfg: MerchantsConfig,
    rng: Rng,
    *,
    start_date: datetime,
    days: int,
    ctx: DayToDayContext,
    infra: TxnInfraAssigner | None = None,
) -> list[Txn]:
    if days <= 0:
        return []

    txf = TxnFactory(rng=rng, infra=infra)

    shares: ArrF64 = np.array(
        [
            float(mcfg.share_merchant),
            float(mcfg.share_bills),
            float(mcfg.share_p2p),
            float(mcfg.share_external_unknown),
        ],
        dtype=np.float64,
    )
    sum_shares_obj: object = cast(object, np.sum(shares, dtype=np.float64))
    sum_shares = _as_float(sum_shares_obj)
    shares = shares / (sum_shares if sum_shares > 0.0 else 1.0)

    cdf_chan_obj: object = cast(object, np.cumsum(shares, dtype=np.float64))
    cdf_chan: ArrF64 = np.asarray(cdf_chan_obj, dtype=np.float64)
    cdf_chan[-1] = 1.0

    base_per_day = float(mcfg.target_payments_per_person_per_month) / 30.0

    explore_base = float(mcfg.explore_p)
    weekend_explore_mult = 1.25
    burst_explore_mult = 3.25

    txns: list[Txn] = []

    persons = ctx.persons
    n_persons = len(persons)

    day_cap = int(ecfg.max_events_per_day)

    for day in range(days):
        day_start = start_date + timedelta(days=day)

        wd_mult = float(weekday_multiplier(day_start, DEFAULT_COUNT_MODELS))
        is_weekend = day_start.weekday() >= 5

        produced_today = 0

        for pi in range(n_persons):
            if day_cap > 0 and produced_today >= day_cap:
                break

            p = persons[pi]
            src = ctx.primary_acct_for_person.get(p)
            if src is None:
                continue

            pname = ctx.persona_for_person.get(p, "salaried")
            persona = PERSONAS.get(pname, PERSONAS["salaried"])

            rate = base_per_day * float(persona.rate_mult) * wd_mult

            n_out_obj: object = cast(
                object,
                gamma_poisson_out_count(
                    rng, base_rate=rate, models=DEFAULT_COUNT_MODELS
                ),
            )
            n_out = _as_int(n_out_obj)
            if n_out <= 0:
                continue

            if day_cap > 0:
                n_out = min(n_out, max(0, day_cap - produced_today))
                if n_out <= 0:
                    break

            offsets_obj: object = cast(
                object,
                sample_offsets_seconds(rng, persona.timing_profile, n_out, ctx.timing),
            )
            offsets: ArrI32 = np.asarray(offsets_obj, dtype=np.int32)

            # --- FIX: avoid float()/int() directly on numpy scalars (Any leakage) ---
            ep = _as_float(cast(object, ctx.explore_propensity[pi]))
            bs = _as_int(cast(object, ctx.burst_start_day[pi]))
            bl = _as_int(cast(object, ctx.burst_len[pi]))
            fk = _as_int(cast(object, ctx.fav_k[pi]))
            bk = _as_int(cast(object, ctx.bill_k[pi]))

            p_explore = explore_base * (0.25 + 0.75 * ep)
            if is_weekend:
                p_explore *= weekend_explore_mult
            if bs >= 0 and bl > 0 and (bs <= day < bs + bl):
                p_explore *= burst_explore_mult

            p_explore = min(0.50, max(0.0, p_explore))

            for i in range(n_out):
                off = _as_int(cast(object, offsets[i]))
                ts = day_start + timedelta(seconds=off)

                u = float(rng.float())
                ch_obj: object = cast(
                    object, np.searchsorted(cdf_chan, u, side="right")
                )
                ch = _as_int(ch_obj)

                if ch == 2:
                    cidx_obj: object = cast(
                        object,
                        ctx.social.contacts[pi, rng.int(0, ctx.social.k_contacts)],
                    )
                    cidx = _as_int(cidx_obj)

                    if 0 <= cidx < n_persons:
                        dst_person = persons[cidx]
                        dst = ctx.primary_acct_for_person.get(dst_person)
                        if dst is None or dst == src:
                            continue

                        amt = _as_float(cast(object, p2p_amount(rng))) * float(
                            persona.amount_mult
                        )
                        amt = round(max(1.0, amt), 2)

                        txns.append(
                            txf.make(
                                TxnSpec(src=src, dst=dst, amt=amt, ts=ts, channel="p2p")
                            )
                        )
                        produced_today += 1

                elif ch == 1:
                    j_obj: object = cast(
                        object, ctx.billers_idx[pi, rng.int(0, max(1, bk))]
                    )
                    j = _as_int(j_obj)
                    dst = ctx.merchants.counterparty_acct[j]

                    amt = _as_float(cast(object, bill_amount(rng)))
                    txns.append(
                        txf.make(
                            TxnSpec(src=src, dst=dst, amt=amt, ts=ts, channel="bill")
                        )
                    )
                    produced_today += 1

                elif ch == 3:
                    if ctx.merchants.external_accounts:
                        dst = rng.choice(ctx.merchants.external_accounts)
                    else:
                        dst = "X0000000001"

                    amt = _as_float(cast(object, p2p_amount(rng)))
                    amt = round(max(1.0, amt), 2)

                    txns.append(
                        txf.make(
                            TxnSpec(
                                src=src,
                                dst=dst,
                                amt=amt,
                                ts=ts,
                                channel="external_unknown",
                            )
                        )
                    )
                    produced_today += 1

                else:
                    exploring = rng.coin(p_explore)

                    if (not exploring) and fk > 0:
                        j_obj2: object = cast(
                            object, ctx.fav_merchants_idx[pi, rng.int(0, fk)]
                        )
                        j = _as_int(j_obj2)
                    else:
                        # exploration: sample global merchant distribution, try to avoid favorites
                        tries = 0
                        j = _cdf_pick_u(ctx.merch_cdf, float(rng.float()))

                        row_obj: object = cast(object, ctx.fav_merchants_idx[pi, :])
                        row: ArrI32 = np.asarray(row_obj, dtype=np.int32)

                        while fk > 0 and _row_contains(row, fk, j) and tries < 6:
                            j = _cdf_pick_u(ctx.merch_cdf, float(rng.float()))
                            tries += 1

                    dst = ctx.merchants.counterparty_acct[j]

                    amt = _as_float(cast(object, p2p_amount(rng))) * float(
                        persona.amount_mult
                    )
                    amt = round(max(1.0, amt), 2)

                    txns.append(
                        txf.make(
                            TxnSpec(
                                src=src, dst=dst, amt=amt, ts=ts, channel="merchant"
                            )
                        )
                    )
                    produced_today += 1

                if day_cap > 0 and produced_today >= day_cap:
                    break

    return txns
