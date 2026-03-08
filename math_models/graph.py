from dataclasses import dataclass
from typing import cast

import numpy as np
import numpy.typing as npt

from common.config import GraphConfig
from common.probability import as_float, build_cdf, cdf_pick
from common.rng import Rng


type NumScalar = float | int | np.floating | np.integer
type ArrF64 = npt.NDArray[np.float64]
type ArrF32 = npt.NDArray[np.float32]
type ArrI32 = npt.NDArray[np.int32]


@dataclass(frozen=True, slots=True)
class NeighborGraph:
    """
    For each src account index i:
      neighbors[i, j] = dst index
      cdf[i, j]       = cumulative probability up to j (last element ~ 1.0)
    """

    neighbors: ArrI32  # shape (n_accounts, k)
    cdf: ArrF32  # shape (n_accounts, k)


def _assign_households(rng: Rng, persons: list[str]) -> dict[str, int]:
    """
    Simple household/community assignment to mimic SBM-like local connectivity.
    Household sizes: 1..5 via clipped geometric-ish.
    """
    out: dict[str, int] = {}
    hid = 0
    i = 0
    n = len(persons)

    while i < n:
        size = 1
        for _ in range(4):
            if rng.coin(0.35):
                size += 1
        if size > 5:
            size = 5

        for _ in range(size):
            if i >= n:
                break
            out[persons[i]] = hid
            i += 1

        hid += 1

    return out


def build_neighbor_graph(
    gcfg: GraphConfig,
    rng: Rng,
    *,
    accounts: list[str],
    acct_owner: dict[str, str],
    hub_accounts: set[str],
) -> NeighborGraph:
    """
    Build a small weighted neighbor set per source account.

    - intra-household edges with probability gcfg.graph_intra_household_p
    - global edges otherwise, sampled from an attractiveness prior (heavy-tailed)
      with hub boost (preferential-ish)
    """
    n = len(accounts)
    if n == 0:
        raise ValueError("accounts must be non-empty")

    k = int(gcfg.graph_k_neighbors)
    if k <= 0:
        raise ValueError("graph_k_neighbors must be > 0")

    acct_index: dict[str, int] = {a: i for i, a in enumerate(accounts)}

    # Keep person order deterministic.
    persons = list(dict.fromkeys(acct_owner[a] for a in accounts if a in acct_owner))
    person_household = _assign_households(rng, persons)

    household_accounts: dict[int, list[int]] = {}
    for acct in accounts:
        owner = acct_owner.get(acct)
        if owner is None:
            continue
        hid = person_household.get(owner, -1)
        household_accounts.setdefault(hid, []).append(acct_index[acct])

    sigma = float(gcfg.graph_attractiveness_sigma)

    attract: ArrF64 = np.asarray(
        cast(object, rng.gen.lognormal(mean=0.0, sigma=sigma, size=n)),
        dtype=np.float64,
    )

    if hub_accounts:
        hub_boost = float(gcfg.graph_hub_weight_boost)
        for hub_acct in hub_accounts:
            idx = acct_index.get(hub_acct)
            if idx is not None:
                attract[idx] *= hub_boost

    cdf_global = build_cdf(attract)

    neighbors: ArrI32 = np.empty((n, k), dtype=np.int32)
    weights: ArrF64 = np.empty((n, k), dtype=np.float64)

    intra_p = float(gcfg.graph_intra_household_p)
    edge_shape = float(gcfg.graph_edge_weight_gamma_shape)

    for i, src in enumerate(accounts):
        owner = acct_owner.get(src)
        hid = person_household.get(owner, -1) if owner is not None else -1
        local_pool = household_accounts.get(hid, [])

        for j in range(k):
            use_local = (len(local_pool) >= 2) and rng.coin(intra_p)

            if use_local:
                dst = i
                tries = 0
                while dst == i and tries < 6:
                    dst = local_pool[rng.int(0, len(local_pool))]
                    tries += 1
            else:
                dst = cdf_pick(cdf_global, rng.float())
                if dst == i:
                    dst = cdf_pick(cdf_global, rng.float())
                    if dst == i:
                        dst = (i + 1) % n

            neighbors[i, j] = dst

            weights[i, j] = as_float(
                cast(
                    NumScalar,
                    rng.gen.gamma(shape=edge_shape, scale=1.0),
                )
            )

        row_sum = as_float(cast(NumScalar, np.sum(weights[i, :], dtype=np.float64)))
        if row_sum <= 0.0 or not np.isfinite(row_sum):
            weights[i, :] = 1.0
            row_sum = float(k)

        weights[i, :] = weights[i, :] / row_sum

    cdf: ArrF32 = np.cumsum(weights, axis=1, dtype=np.float64).astype(np.float32)
    cdf[:, -1] = 1.0

    return NeighborGraph(neighbors=neighbors, cdf=cdf)
