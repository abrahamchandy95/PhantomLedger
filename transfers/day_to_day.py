from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

import numpy as np
import numpy.typing as npt

from common.config import CreditConfig, EventsConfig, MerchantsConfig
from common.probability import as_float, as_int, build_cdf, cdf_pick
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


type NumScalar = float | int | np.floating | np.integer
type ArrF64 = npt.NDArray[np.float64]
type ArrI32 = npt.NDArray[np.int32]
type ArrI16 = npt.NDArray[np.int16]
type ArrF32 = npt.NDArray[np.float32]
type ArrBool = npt.NDArray[np.bool_]


def _pick_unique_weighted(gen: np.random.Generator, cdf: ArrF64, k: int) -> list[int]:
    out: list[int] = []
    seen: set[int] = set()
    tries = 0
    max_tries = 250

    while len(out) < k and tries < max_tries:
        j = cdf_pick(cdf, float(gen.random()))
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
        if as_int(cast(int | np.integer, arr_row[i])) == v:
            return True
    return False


def _build_channel_cdf(ecfg: EventsConfig, mcfg: MerchantsConfig) -> ArrF64:
    """
    Channel mix:
      - merchant
      - bills
      - p2p
      - external_unknown

    unknown_outflow_p is treated as the final probability of the
    external_unknown channel. The other three shares are renormalized to
    fill the remaining mass.
    """
    unknown_p = min(1.0, max(0.0, float(ecfg.unknown_outflow_p)))

    core = np.array(
        [
            float(mcfg.share_merchant),
            float(mcfg.share_bills),
            float(mcfg.share_p2p),
        ],
        dtype=np.float64,
    )

    core_sum = as_float(cast(NumScalar, np.sum(core, dtype=np.float64)))
    if not np.isfinite(core_sum) or core_sum <= 0.0:
        core[:] = 1.0
        core_sum = float(core.size)

    core = core / core_sum

    shares = np.array(
        [
            (1.0 - unknown_p) * core[0],
            (1.0 - unknown_p) * core[1],
            (1.0 - unknown_p) * core[2],
            unknown_p,
        ],
        dtype=np.float64,
    )

    return build_cdf(shares)


@dataclass(frozen=True, slots=True)
class DayToDayContext:
    persons: list[str]
    people_index: dict[str, int]

    primary_acct_for_person: dict[str, str]
    persona_for_person: dict[str, str]

    timing: TimingProfiles

    merchants: MerchantData
    merch_cdf: ArrF64
    global_biller_cdf: ArrF64

    social: SocialGraph

    fav_merchants_idx: ArrI32
    fav_k: ArrI16

    billers_idx: ArrI32
    bill_k: ArrI16

    explore_propensity: ArrF32
    burst_start_day: ArrI32
    burst_len: ArrI16


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
    _ = start_date.year
    timing = default_timing_profiles()

    merch_w: ArrF64 = np.asarray(merchants.weight, dtype=np.float64)
    merch_cdf = build_cdf(merch_w)

    biller_cats: set[str] = {"utilities", "telecom", "insurance", "education"}
    mask_list: list[bool] = [c in biller_cats for c in merchants.category]
    mask: ArrBool = np.asarray(mask_list, dtype=np.bool_)

    biller_w: ArrF64 = merch_w * mask.astype(np.float64)
    biller_sum = as_float(cast(NumScalar, np.sum(biller_w, dtype=np.float64)))
    global_biller_cdf = build_cdf(biller_w) if biller_sum > 0.0 else merch_cdf

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

        fk = as_int(
            cast(
                int | np.integer,
                g.integers(
                    int(mcfg.fav_merchants_min), int(mcfg.fav_merchants_max) + 1
                ),
            )
        )
        bk = as_int(
            cast(
                int | np.integer,
                g.integers(int(mcfg.billers_min), int(mcfg.billers_max) + 1),
            )
        )

        fav = _pick_unique_weighted(g, merch_cdf, fk)
        bill = _pick_unique_weighted(g, global_biller_cdf, bk)

        fav_k[i] = np.int16(fk)
        bill_k[i] = np.int16(bk)

        fav_idx[i, :] = int(fav[0])
        fav_idx[i, : len(fav)] = np.asarray(fav, dtype=np.int32)

        bill_idx[i, :] = int(bill[0])
        bill_idx[i, : len(bill)] = np.asarray(bill, dtype=np.int32)

        alpha = 1.6
        beta = 9.5
        explore_prop[i] = np.float32(float(g.beta(alpha, beta)))

        if days > 0 and float(g.random()) < burst_p:
            bs = as_int(cast(int | np.integer, g.integers(0, max(1, days))))
            bl = as_int(
                cast(
                    int | np.integer,
                    g.integers(burst_len_min, burst_len_max + 1),
                )
            )
            burst_start[i] = bs
            burst_len[i] = np.int16(bl)

    _ = (rng.float(), float(ecfg.prefer_billers_p))

    return DayToDayContext(
        persons=persons,
        people_index=people_index,
        primary_acct_for_person=primary_acct_for_person,
        persona_for_person=persona_for_person,
        timing=timing,
        merchants=merchants,
        merch_cdf=merch_cdf,
        global_biller_cdf=global_biller_cdf,
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
    credit_cfg: CreditConfig | None = None,
    card_for_person: dict[str, str] | None = None,
) -> list[Txn]:
    if days <= 0:
        return []

    txf = TxnFactory(rng=rng, infra=infra)
    cdf_chan = _build_channel_cdf(ecfg, mcfg)

    base_per_day = float(mcfg.target_payments_per_person_per_month) / 30.0
    prefer_billers_p = float(ecfg.prefer_billers_p)

    explore_base = float(mcfg.explore_p)
    weekend_explore_mult = 1.25
    burst_explore_mult = 3.25

    txns: list[Txn] = []

    persons = ctx.persons
    n_persons = len(persons)

    day_cap = int(ecfg.max_events_per_day)

    ccfg: CreditConfig | None = None
    card_map: dict[str, str] = {}

    if credit_cfg is not None and credit_cfg.enable_credit_cards:
        ccfg = credit_cfg
        if card_for_person is not None:
            card_map = card_for_person

    for day in range(days):
        day_start = start_date + timedelta(days=day)

        wd_mult = float(weekday_multiplier(day_start, DEFAULT_COUNT_MODELS))
        is_weekend = day_start.weekday() >= 5

        k = float(ecfg.day_multiplier_gamma_shape)
        day_shock = float(rng.gen.gamma(shape=k, scale=(1.0 / k)))

        produced_today = 0

        for pi in range(n_persons):
            if day_cap > 0 and produced_today >= day_cap:
                break

            p = persons[pi]
            dep_acct = ctx.primary_acct_for_person.get(p)
            if dep_acct is None:
                continue

            pname = ctx.persona_for_person.get(p, "salaried")
            persona = PERSONAS.get(pname, PERSONAS["salaried"])

            rate = base_per_day * float(persona.rate_mult) * wd_mult * day_shock

            n_out = gamma_poisson_out_count(
                rng,
                base_rate=rate,
                models=DEFAULT_COUNT_MODELS,
            )
            if n_out <= 0:
                continue

            if day_cap > 0:
                n_out = min(n_out, max(0, day_cap - produced_today))
                if n_out <= 0:
                    break

            offsets: ArrI32 = np.asarray(
                sample_offsets_seconds(rng, persona.timing_profile, n_out, ctx.timing),
                dtype=np.int32,
            )

            ep = as_float(cast(NumScalar, ctx.explore_propensity[pi]))
            bs = as_int(cast(int | np.integer, ctx.burst_start_day[pi]))
            bl = as_int(cast(int | np.integer, ctx.burst_len[pi]))
            fk = as_int(cast(int | np.integer, ctx.fav_k[pi]))
            bk = as_int(cast(int | np.integer, ctx.bill_k[pi]))

            p_explore = explore_base * (0.25 + 0.75 * ep)
            if is_weekend:
                p_explore *= weekend_explore_mult
            if bs >= 0 and bl > 0 and (bs <= day < bs + bl):
                p_explore *= burst_explore_mult
            p_explore = min(0.50, max(0.0, p_explore))

            for i in range(n_out):
                off = as_int(cast(int | np.integer, offsets[i]))
                ts = day_start + timedelta(seconds=off)

                ch = cdf_pick(cdf_chan, rng.float())

                if ch == 2:
                    cidx = as_int(
                        cast(
                            int | np.integer,
                            ctx.social.contacts[pi, rng.int(0, ctx.social.k_contacts)],
                        )
                    )
                    if 0 <= cidx < n_persons:
                        dst_person = persons[cidx]
                        dst = ctx.primary_acct_for_person.get(dst_person)
                        if dst is None or dst == dep_acct:
                            continue

                        amt = float(p2p_amount(rng)) * float(persona.amount_mult)
                        amt = round(max(1.0, amt), 2)

                        txns.append(
                            txf.make(
                                TxnSpec(
                                    src=dep_acct,
                                    dst=dst,
                                    amt=amt,
                                    ts=ts,
                                    channel="p2p",
                                )
                            )
                        )
                        produced_today += 1

                elif ch == 1:
                    if bk > 0 and rng.coin(prefer_billers_p):
                        j = as_int(
                            cast(
                                int | np.integer,
                                ctx.billers_idx[pi, rng.int(0, max(1, bk))],
                            )
                        )
                    else:
                        j = cdf_pick(ctx.global_biller_cdf, rng.float())

                    dst = ctx.merchants.counterparty_acct[j]
                    amt = float(bill_amount(rng))

                    txns.append(
                        txf.make(
                            TxnSpec(
                                src=dep_acct,
                                dst=dst,
                                amt=amt,
                                ts=ts,
                                channel="bill",
                            )
                        )
                    )
                    produced_today += 1

                elif ch == 3:
                    if ctx.merchants.external_accounts:
                        dst = rng.choice(ctx.merchants.external_accounts)
                    else:
                        dst = "X0000000001"

                    amt = float(p2p_amount(rng))
                    amt = round(max(1.0, amt), 2)

                    txns.append(
                        txf.make(
                            TxnSpec(
                                src=dep_acct,
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
                        j = as_int(
                            cast(
                                int | np.integer,
                                ctx.fav_merchants_idx[pi, rng.int(0, fk)],
                            )
                        )
                    else:
                        tries = 0
                        j = cdf_pick(ctx.merch_cdf, rng.float())
                        row: ArrI32 = np.asarray(
                            ctx.fav_merchants_idx[pi, :],
                            dtype=np.int32,
                        )
                        while fk > 0 and _row_contains(row, fk, j) and tries < 6:
                            j = cdf_pick(ctx.merch_cdf, rng.float())
                            tries += 1

                    dst = ctx.merchants.counterparty_acct[j]

                    amt = float(p2p_amount(rng)) * float(persona.amount_mult)
                    amt = round(max(1.0, amt), 2)

                    src_acct = dep_acct
                    channel = "merchant"

                    if ccfg is not None:
                        card = card_map.get(p)
                        if card is not None:
                            p_cc = float(ccfg.cc_share_for_merchant(pname))
                            if rng.coin(p_cc):
                                src_acct = card
                                channel = "card_purchase"

                    txns.append(
                        txf.make(
                            TxnSpec(
                                src=src_acct,
                                dst=dst,
                                amt=amt,
                                ts=ts,
                                channel=channel,
                            )
                        )
                    )
                    produced_today += 1

                if day_cap > 0 and produced_today >= day_cap:
                    break

    return txns
