from collections.abc import Sequence
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
from .environment import (
    BurstBehavior,
    CommercialNetwork,
    ExplorationHabits,
    Market,
    MerchantView,
    Parameters,
    PopulationCensus,
    PopulationView,
    SimulationBounds,
)


def _unique_weighted_pick(
    gen: np.random.Generator,
    cdf: F64,
    k: int,
    max_tries: int,
) -> list[int]:
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


def _build_merch_cdf(weights_array: F64) -> F64:
    weights: F64 = np.asarray(weights_array, dtype=np.float64)
    return build_cdf(weights)


def _build_biller_cdf(
    weights_array: F64,
    categories_array: Sequence[str],
    biller_categories: tuple[str, ...],
    merch_cdf: F64,
) -> F64:
    categories = set(biller_categories)
    mask_list = [c in categories for c in categories_array]
    mask: Bool = np.asarray(mask_list, dtype=np.bool_)

    weights: F64 = np.asarray(weights_array, dtype=np.float64)
    biller_w: F64 = weights * mask.astype(np.float64)

    if as_float(cast(Scalar, np.sum(biller_w))) > 0.0:
        return build_cdf(biller_w)
    return merch_cdf


def _roll_counts(
    rngs: list[np.random.Generator],
    min_val: int,
    max_val: int,
) -> I16:
    n = len(rngs)
    counts: I16 = np.empty(n, dtype=np.int16)
    for i, gen in enumerate(rngs):
        counts[i] = np.int16(gen.integers(min_val, max_val + 1))
    return counts


def _build_pick_matrix(
    rngs: list[np.random.Generator],
    k_array: I16,
    cdf: F64,
    max_cols: int,
    max_tries: int,
) -> I32:
    n = len(rngs)
    matrix: I32 = np.empty((n, max_cols), dtype=np.int32)
    for i, gen in enumerate(rngs):
        k = int(k_array.item(i))
        picks = _unique_weighted_pick(gen, cdf, k, max_tries)

        matrix[i, :] = picks[0]  # Fill entire row with primary pick as fallback

        matrix[i, : len(picks)] = picks
    return matrix


def _roll_behaviors(
    rngs: list[np.random.Generator],
    explore_habits: ExplorationHabits,
    burst: BurstBehavior,
    days: int,
) -> tuple[F32, I32, I16]:
    n = len(rngs)
    explore: F32 = np.empty(n, dtype=np.float32)
    burst_start: I32 = np.full(n, -1, dtype=np.int32)
    burst_len: I16 = np.zeros(n, dtype=np.int16)

    for i, gen in enumerate(rngs):
        explore[i] = np.float32(gen.beta(explore_habits.alpha, explore_habits.beta))
        if days > 0 and gen.random() < burst.probability:
            burst_start[i] = as_int(gen.integers(0, days))
            burst_len[i] = np.int16(gen.integers(burst.min_days, burst.max_days + 1))

    return explore, burst_start, burst_len


def _build_population_view(census: PopulationCensus) -> PopulationView:
    return PopulationView(
        persons=census.persons,
        people_index={pid: i for i, pid in enumerate(census.persons)},
        primary_accounts=census.primary_accounts,
        personas=census.personas,
        persona_objects=census.persona_objects,
        timing=DEFAULT_PROFILES,
    )


def _build_merchant_view(
    network: CommercialNetwork,
    bounds: SimulationBounds,
    persons: list[str],
    params: Parameters,
    base_seed: int,
) -> MerchantView:
    merch_cdf = _build_merch_cdf(network.merchants.weights)
    biller_cdf = _build_biller_cdf(
        network.merchants.weights,
        network.merchants.categories,
        params.picking_rules.biller_categories,
        merch_cdf,
    )

    rngs = [
        np.random.default_rng(derive_seed(base_seed, "payees", pid)) for pid in persons
    ]

    fav_k = _roll_counts(
        rngs,
        int(bounds.merchants_cfg.favorite_min),
        int(bounds.merchants_cfg.favorite_max),
    )
    bill_k = _roll_counts(
        rngs, int(bounds.merchants_cfg.biller_min), int(bounds.merchants_cfg.biller_max)
    )

    fav_idx = _build_pick_matrix(
        rngs,
        fav_k,
        merch_cdf,
        int(bounds.merchants_cfg.favorite_max),
        params.picking_rules.max_pick_attempts,
    )
    bill_idx = _build_pick_matrix(
        rngs,
        bill_k,
        biller_cdf,
        int(bounds.merchants_cfg.biller_max),
        params.picking_rules.max_pick_attempts,
    )

    explore, burst_start, burst_len = _roll_behaviors(
        rngs, params.exploration_habits, params.burst_behavior, bounds.days
    )

    return MerchantView(
        merchants=network.merchants,
        merch_cdf=merch_cdf,
        biller_cdf=biller_cdf,
        fav_merchants_idx=fav_idx,
        fav_k=fav_k,
        billers_idx=bill_idx,
        bill_k=bill_k,
        explore_propensity=explore,
        burst_start_day=burst_start,
        burst_len=burst_len,
    )


def build_market(
    bounds: SimulationBounds,
    census: PopulationCensus,
    network: CommercialNetwork,
    params: Parameters,
    base_seed: int,
) -> Market:
    """
    Orchestrates pure domain builders into an active Market ecosystem.
    """
    pop_view = _build_population_view(census)
    merch_view = _build_merchant_view(
        network, bounds, census.persons, params, base_seed
    )

    dynamics = initialize_dynamics(len(census.persons))
    paydays = tuple(
        census.paydays_by_person.get(pid, frozenset()) for pid in census.persons
    )

    return Market(
        start_date=bounds.start_date,
        days=bounds.days,
        events=bounds.events,
        merchants_cfg=bounds.merchants_cfg,
        population=pop_view,
        merchants=merch_view,
        social=network.social,
        person_states=dynamics,
        paydays=paydays,
    )
