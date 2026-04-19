"""
Landlord pool builder.

Generates the typed landlord counterparty universe used by rent generation.
Each landlord is assigned exactly one type drawn from the unit-weighted
distribution in `common.config.population.landlords.Landlords.type_shares`,
then independently assigned as internal (on-us) or external based on the
per-type `in_bank_p` probability.

This module deliberately does not know anything about payment channels or
rent amounts. Its only job is to produce `entities.models.Landlords` with
stable, deterministic account IDs partitioned by type and banking
relationship. Channel selection happens later in
`transfers.legit.inflows.rent_routing`.
"""

from collections.abc import Callable

from common import config
from common.ids import (
    landlord_corporate_id,
    landlord_corporate_external_id,
    landlord_individual_id,
    landlord_individual_external_id,
    landlord_small_llc_id,
    landlord_small_llc_external_id,
)
from common.landlord_types import CORPORATE, INDIVIDUAL, LLC_SMALL
from common.math import build_cdf, cdf_pick
from common.random import Rng

from . import models


# Per-type ID factories. Internal factories produce non-X-prefixed IDs
# (LI/LS/LC), external factories produce X-prefixed IDs (XLI/XLS/XLC).
# Adding a new landlord type requires entries in both dicts *and* a
# corresponding entry in common.config.population.landlords.Landlords.
_INTERNAL_ID_FACTORY: dict[str, Callable[[int], str]] = {
    INDIVIDUAL: landlord_individual_id,
    LLC_SMALL: landlord_small_llc_id,
    CORPORATE: landlord_corporate_id,
}

_EXTERNAL_ID_FACTORY: dict[str, Callable[[int], str]] = {
    INDIVIDUAL: landlord_individual_external_id,
    LLC_SMALL: landlord_small_llc_external_id,
    CORPORATE: landlord_corporate_external_id,
}


def _minimum_pool_size(density_per_10k: float, population: int) -> int:
    """
    Floor at three so downstream lease rotations (across multiple lease moves
    in the same simulation window) always have room to pick a different
    landlord from the previous one.
    """
    scaled = int(round(density_per_10k * (population / 10_000.0)))
    return max(3, scaled)


def build(
    cfg: config.Landlords,
    pop_cfg: config.Population,
    rng: Rng,
) -> models.Landlords:
    """
    Build the landlord universe for the current simulation world.

    The pool size is driven by `cfg.per_10k_people` and population size.
    Each landlord's type is drawn from `cfg.type_shares` via a shared CDF.
    Whether the landlord banks at our institution is drawn independently
    from `cfg.in_bank_p[type]`. Internal and external landlords get
    separate ID counters and prefix families so IDs never collide.
    """
    n_total = _minimum_pool_size(float(cfg.per_10k_people), int(pop_cfg.size))

    # Fix the type ordering once so both the CDF and by_type reflect the same
    # iteration order across runs with the same seed.
    type_names: list[str] = list(cfg.type_shares.keys())
    weights = [float(cfg.type_shares[t]) for t in type_names]
    type_cdf = build_cdf(weights)

    # Guard against config omissions at build time rather than at emission.
    missing_int = [t for t in type_names if t not in _INTERNAL_ID_FACTORY]
    missing_ext = [t for t in type_names if t not in _EXTERNAL_ID_FACTORY]
    if missing_int or missing_ext:
        raise ValueError(
            "Landlords config references types without ID factories: "
            + f"internal={missing_int}, external={missing_ext}"
        )

    by_type: dict[str, list[str]] = {t: [] for t in type_names}
    # Separate counters for internal and external to avoid ID collisions.
    internal_counters: dict[str, int] = {t: 0 for t in type_names}
    external_counters: dict[str, int] = {t: 0 for t in type_names}
    type_of: dict[str, str] = {}
    records: list[models.Landlord] = []
    ids: list[str] = []
    internals: list[str] = []
    externals: list[str] = []

    for _ in range(n_total):
        type_idx = cdf_pick(type_cdf, rng.float())
        ltype = type_names[type_idx]

        # Determine banking relationship.
        in_bank_p = float(cfg.in_bank_p.get(ltype, 0.0))
        is_internal = rng.coin(in_bank_p)

        if is_internal:
            internal_counters[ltype] += 1
            acct_id = _INTERNAL_ID_FACTORY[ltype](internal_counters[ltype])
            internals.append(acct_id)
        else:
            external_counters[ltype] += 1
            acct_id = _EXTERNAL_ID_FACTORY[ltype](external_counters[ltype])
            externals.append(acct_id)

        ids.append(acct_id)
        by_type[ltype].append(acct_id)
        type_of[acct_id] = ltype
        records.append(models.Landlord(account_id=acct_id, type=ltype))

    return models.Landlords(
        ids=ids,
        by_type=by_type,
        type_of=type_of,
        records=records,
        internals=internals,
        externals=externals,
    )
