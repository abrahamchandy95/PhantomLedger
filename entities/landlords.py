"""
Landlord pool builder.

Generates the typed landlord counterparty universe used by rent generation.
Each landlord is assigned exactly one type drawn from the unit-weighted
distribution in `common.config.population.landlords.Landlords.type_shares`.

This module deliberately does not know anything about payment channels or
rent amounts. Its only job is to produce `entities.models.Landlords` with
stable, deterministic account IDs partitioned by type. Channel selection
happens later in `transfers.legit.inflows.rent_routing`.
"""

from collections.abc import Callable

from common import config
from common.ids import (
    landlord_corporate_id,
    landlord_individual_id,
    landlord_small_llc_id,
)
from common.landlord_types import CORPORATE, INDIVIDUAL, LLC_SMALL
from common.math import build_cdf, cdf_pick
from common.random import Rng

from . import models


# Per-type ID factories. The dict is closed at import time; adding a new
# landlord type requires adding a new factory here *and* a corresponding
# entry in common.config.population.landlords.Landlords.channel_mix.
_ID_FACTORY: dict[str, Callable[[int], str]] = {
    INDIVIDUAL: landlord_individual_id,
    LLC_SMALL: landlord_small_llc_id,
    CORPORATE: landlord_corporate_id,
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
    Each landlord's type is drawn from `cfg.type_shares` via a shared CDF,
    and the per-type counter ensures deterministic IDs like XLI0000001,
    XLS0000001, XLC0000001 regardless of draw order.
    """
    n_total = _minimum_pool_size(float(cfg.per_10k_people), int(pop_cfg.size))

    # Fix the type ordering once so both the CDF and by_type reflect the same
    # iteration order across runs with the same seed.
    type_names: list[str] = list(cfg.type_shares.keys())
    weights = [float(cfg.type_shares[t]) for t in type_names]
    type_cdf = build_cdf(weights)

    # Guard against config omissions at build time rather than at emission.
    missing_factories = [t for t in type_names if t not in _ID_FACTORY]
    if missing_factories:
        raise ValueError(
            "Landlords config references types without an ID factory: "
            + f"{missing_factories}"
        )

    by_type: dict[str, list[str]] = {t: [] for t in type_names}
    counters: dict[str, int] = {t: 0 for t in type_names}
    type_of: dict[str, str] = {}
    records: list[models.Landlord] = []
    ids: list[str] = []

    for _ in range(n_total):
        type_idx = cdf_pick(type_cdf, rng.float())
        ltype = type_names[type_idx]

        counters[ltype] += 1
        acct_id = _ID_FACTORY[ltype](counters[ltype])

        ids.append(acct_id)
        by_type[ltype].append(acct_id)
        type_of[acct_id] = ltype
        records.append(models.Landlord(account_id=acct_id, type=ltype))

    return models.Landlords(
        ids=ids,
        by_type=by_type,
        type_of=type_of,
        records=records,
    )
