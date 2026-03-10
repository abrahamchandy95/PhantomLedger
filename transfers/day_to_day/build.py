from dataclasses import dataclass
from typing import cast

import numpy as np

from common.math import (
    ArrBool,
    ArrF32,
    ArrF64,
    ArrI16,
    ArrI32,
    NumScalar,
    as_float,
    as_int,
    build_cdf,
    cdf_pick,
)
from common.random import derived_seed

from math_models.timing import default_timing_profiles

from .models import DayToDayBuildRequest, DayToDayContext


@dataclass(frozen=True, slots=True)
class _PayeeState:
    fav_merchants_idx: ArrI32
    fav_k: ArrI16
    billers_idx: ArrI32
    bill_k: ArrI16
    explore_propensity: ArrF32
    burst_start_day: ArrI32
    burst_len: ArrI16


def _pick_unique_weighted(
    gen: np.random.Generator,
    cdf: ArrF64,
    k: int,
    max_tries: int,
) -> list[int]:
    out: list[int] = []
    seen: set[int] = set()
    tries = 0

    while len(out) < k and tries < max_tries:
        picked_idx = cdf_pick(cdf, float(gen.random()))
        tries += 1
        if picked_idx in seen:
            continue
        seen.add(picked_idx)
        out.append(picked_idx)

    if not out:
        out.append(0)
    return out


@dataclass(slots=True)
class _DayToDayContextBuilder:
    request: DayToDayBuildRequest

    def build(self) -> DayToDayContext:
        self.request.policy.validate()
        _ = self.request.start_date.year

        timing = default_timing_profiles()
        merch_cdf = self._merch_cdf()
        global_biller_cdf = self._global_biller_cdf(merch_cdf)
        people_index = {
            person_id: idx for idx, person_id in enumerate(self.request.persons)
        }
        payee_state = self._build_payee_state(merch_cdf, global_biller_cdf)

        _ = (self.request.rng.float(), float(self.request.events.prefer_billers_p))

        return DayToDayContext(
            persons=self.request.persons,
            people_index=people_index,
            primary_acct_for_person=self.request.primary_acct_for_person,
            persona_for_person=self.request.persona_for_person,
            timing=timing,
            merchants=self.request.merchants,
            merch_cdf=merch_cdf,
            global_biller_cdf=global_biller_cdf,
            social=self.request.social,
            fav_merchants_idx=payee_state.fav_merchants_idx,
            fav_k=payee_state.fav_k,
            billers_idx=payee_state.billers_idx,
            bill_k=payee_state.bill_k,
            explore_propensity=payee_state.explore_propensity,
            burst_start_day=payee_state.burst_start_day,
            burst_len=payee_state.burst_len,
        )

    def _merch_cdf(self) -> ArrF64:
        merch_w: ArrF64 = np.asarray(self.request.merchants.weight, dtype=np.float64)
        return build_cdf(merch_w)

    def _global_biller_cdf(self, merch_cdf: ArrF64) -> ArrF64:
        biller_categories = set(self.request.policy.biller_categories)
        mask_list: list[bool] = [
            category in biller_categories
            for category in self.request.merchants.category
        ]
        mask: ArrBool = np.asarray(mask_list, dtype=np.bool_)

        merch_w: ArrF64 = np.asarray(self.request.merchants.weight, dtype=np.float64)
        biller_w: ArrF64 = merch_w * mask.astype(np.float64)
        biller_sum = as_float(cast(NumScalar, np.sum(biller_w, dtype=np.float64)))

        if biller_sum > 0.0:
            return build_cdf(biller_w)
        return merch_cdf

    def _build_payee_state(
        self,
        merch_cdf: ArrF64,
        global_biller_cdf: ArrF64,
    ) -> _PayeeState:
        n_people = len(self.request.persons)
        mcfg = self.request.merchants_cfg
        policy = self.request.policy

        fav_max = int(mcfg.fav_merchants_max)
        bill_max = int(mcfg.billers_max)

        fav_idx: ArrI32 = np.empty((n_people, fav_max), dtype=np.int32)
        fav_k: ArrI16 = np.empty(n_people, dtype=np.int16)

        bill_idx: ArrI32 = np.empty((n_people, bill_max), dtype=np.int32)
        bill_k: ArrI16 = np.empty(n_people, dtype=np.int16)

        explore_prop: ArrF32 = np.empty(n_people, dtype=np.float32)
        burst_start: ArrI32 = np.full(n_people, -1, dtype=np.int32)
        burst_len: ArrI16 = np.zeros(n_people, dtype=np.int16)

        for person_idx, person_id in enumerate(self.request.persons):
            gen = np.random.default_rng(
                derived_seed(self.request.base_seed, "payees", person_id)
            )

            favorite_count = as_int(
                cast(
                    int | np.integer,
                    gen.integers(
                        int(mcfg.fav_merchants_min),
                        int(mcfg.fav_merchants_max) + 1,
                    ),
                )
            )
            biller_count = as_int(
                cast(
                    int | np.integer,
                    gen.integers(int(mcfg.billers_min), int(mcfg.billers_max) + 1),
                )
            )

            favorites = _pick_unique_weighted(
                gen,
                merch_cdf,
                favorite_count,
                int(policy.unique_pick_max_tries),
            )
            billers = _pick_unique_weighted(
                gen,
                global_biller_cdf,
                biller_count,
                int(policy.unique_pick_max_tries),
            )

            fav_k[person_idx] = np.int16(favorite_count)
            bill_k[person_idx] = np.int16(biller_count)

            fav_idx[person_idx, :] = int(favorites[0])
            fav_idx[person_idx, : len(favorites)] = np.asarray(
                favorites,
                dtype=np.int32,
            )

            bill_idx[person_idx, :] = int(billers[0])
            bill_idx[person_idx, : len(billers)] = np.asarray(
                billers,
                dtype=np.int32,
            )

            explore_prop[person_idx] = np.float32(
                float(
                    gen.beta(
                        float(policy.explore_beta_alpha),
                        float(policy.explore_beta_beta),
                    )
                )
            )

            if self.request.days > 0 and float(gen.random()) < float(
                policy.burst_probability
            ):
                burst_day = as_int(
                    cast(
                        int | np.integer,
                        gen.integers(0, max(1, self.request.days)),
                    )
                )
                burst_days = as_int(
                    cast(
                        int | np.integer,
                        gen.integers(
                            int(policy.burst_len_min),
                            int(policy.burst_len_max) + 1,
                        ),
                    )
                )
                burst_start[person_idx] = burst_day
                burst_len[person_idx] = np.int16(burst_days)

        return _PayeeState(
            fav_merchants_idx=fav_idx,
            fav_k=fav_k,
            billers_idx=bill_idx,
            bill_k=bill_k,
            explore_propensity=explore_prop,
            burst_start_day=burst_start,
            burst_len=burst_len,
        )


def build_day_to_day_context(request: DayToDayBuildRequest) -> DayToDayContext:
    return _DayToDayContextBuilder(request=request).build()
