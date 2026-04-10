from dataclasses import dataclass
from typing import cast

import numpy as np

from common.math import (
    Bool,
    F32,
    F64,
    I16,
    I32,
    Scalar,
    as_float,
    as_int,
    build_cdf,
    cdf_pick,
)
from common.random import derive_seed
from math_models.timing import DEFAULT_PROFILES

from .dynamics import initialize_dynamics
from .engine import BuildRequest, Context, MerchantView, PopulationView


@dataclass(frozen=True, slots=True)
class _PayeeState:
    """Internal container for vectorized payee attributes."""

    fav_idx: I32
    fav_k: I16
    bill_idx: I32
    bill_k: I16
    explore_propensity: F32
    burst_start: I32
    burst_len: I16


def _unique_weighted_pick(
    gen: np.random.Generator,
    cdf: F64,
    k: int,
    max_tries: int,
) -> list[int]:
    """Picks k unique indices from a CDF with a retry limit."""
    out: list[int] = []
    seen: set[int] = set()
    tries = 0

    while len(out) < k and tries < max_tries:
        idx = cdf_pick(cdf, float(gen.random()))
        tries += 1
        if idx not in seen:
            seen.add(idx)
            out.append(idx)

    return out if out else [0]


class _ContextBuilder:
    """Orchestrates the construction of the generation context."""

    request: BuildRequest

    def __init__(self, request: BuildRequest):
        self.request = request

    def build(self) -> Context:
        merch_cdf = self._build_merch_cdf()
        biller_cdf = self._build_biller_cdf(merch_cdf)

        people_index = {pid: i for i, pid in enumerate(self.request.persons)}
        state = self._compute_payee_state(merch_cdf, biller_cdf)

        paydays = tuple(
            self.request.paydays_by_person.get(person_id, frozenset())
            for person_id in self.request.persons
        )
        n_people = len(self.request.persons)
        dynamics = initialize_dynamics(n_people)

        return Context(
            population=PopulationView(
                persons=self.request.persons,
                people_index=people_index,
                primary_accounts=self.request.primary_accounts,
                personas=self.request.personas,
                persona_objects=self.request.persona_objects,
                timing=DEFAULT_PROFILES,
            ),
            merchant=MerchantView(
                merchants=self.request.merchants,
                merch_cdf=merch_cdf,
                biller_cdf=biller_cdf,
                fav_merchants_idx=state.fav_idx,
                fav_k=state.fav_k,
                billers_idx=state.bill_idx,
                bill_k=state.bill_k,
                explore_propensity=state.explore_propensity,
                burst_start_day=state.burst_start,
                burst_len=state.burst_len,
            ),
            social=self.request.social,
            person_dynamics=dynamics,
            paydays_by_person=paydays,
        )

    def _build_merch_cdf(self) -> F64:
        weights: F64 = np.asarray(self.request.merchants.weights, dtype=np.float64)
        return build_cdf(weights)

    def _build_biller_cdf(self, merch_cdf: F64) -> F64:
        categories = set(self.request.params.biller_categories)
        mask_list = [c in categories for c in self.request.merchants.categories]
        mask: Bool = np.asarray(mask_list, dtype=np.bool_)

        weights: F64 = np.asarray(self.request.merchants.weights, dtype=np.float64)
        biller_w: F64 = weights * mask.astype(np.float64)

        if as_float(cast(Scalar, np.sum(biller_w))) > 0.0:
            return build_cdf(biller_w)
        return merch_cdf

    def _compute_payee_state(self, merch_cdf: F64, biller_cdf: F64) -> _PayeeState:
        n = len(self.request.persons)
        cfg = self.request.merchants_cfg
        policy = self.request.params

        fav_idx: I32 = np.empty((n, int(cfg.favorite_max)), dtype=np.int32)
        fav_k: I16 = np.empty(n, dtype=np.int16)
        bill_idx: I32 = np.empty((n, int(cfg.biller_max)), dtype=np.int32)
        bill_k: I16 = np.empty(n, dtype=np.int16)
        explore: F32 = np.empty(n, dtype=np.float32)
        burst_start: I32 = np.full(n, -1, dtype=np.int32)
        burst_len: I16 = np.zeros(n, dtype=np.int16)

        for i, person_id in enumerate(self.request.persons):
            gen = np.random.default_rng(
                derive_seed(self.request.base_seed, "payees", person_id)
            )

            fk = as_int(gen.integers(int(cfg.favorite_min), int(cfg.favorite_max) + 1))
            bk = as_int(gen.integers(int(cfg.biller_min), int(cfg.biller_max) + 1))

            fav_k[i], bill_k[i] = np.int16(fk), np.int16(bk)

            favs = _unique_weighted_pick(gen, merch_cdf, fk, policy.pick_max_tries)
            bills = _unique_weighted_pick(gen, biller_cdf, bk, policy.pick_max_tries)

            fav_idx[i, :] = favs[0]
            fav_idx[i, : len(favs)] = np.asarray(favs, dtype=np.int32)

            bill_idx[i, :] = bills[0]
            bill_idx[i, : len(bills)] = np.asarray(bills, dtype=np.int32)

            explore[i] = np.float32(gen.beta(policy.explore_alpha, policy.explore_beta))

            if self.request.days > 0 and gen.random() < policy.burst_p:
                burst_start[i] = as_int(gen.integers(0, self.request.days))
                burst_len[i] = np.int16(
                    gen.integers(policy.burst_min, policy.burst_max + 1)
                )

        return _PayeeState(
            fav_idx, fav_k, bill_idx, bill_k, explore, burst_start, burst_len
        )


def build_context(request: BuildRequest) -> Context:
    """Top-level entry point to initialize the daily generation environment."""
    return _ContextBuilder(request).build()
