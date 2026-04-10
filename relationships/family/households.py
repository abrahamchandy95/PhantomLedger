from dataclasses import dataclass
from typing import cast

import numpy as np

from common import config
from common.math import F64, build_cdf


@dataclass(frozen=True, slots=True)
class Partition:
    """Intermediate state holding physical household assignments."""

    households: list[list[str]]
    household_map: dict[str, int]


def _build_size_cdf(alpha: float, max_size: int) -> F64:
    sizes = np.arange(2, max_size + 1, dtype=np.float64)
    weights = sizes ** (-float(alpha))
    return build_cdf(weights)


def _sample_sizes(
    cfg: config.Family,
    gen: np.random.Generator,
    cdf_2plus: F64,
    n_people: int,
) -> list[int]:
    """Bulk-samples household sizes"""
    if n_people == 0:
        return []

    rands = gen.random(n_people)
    is_single = rands < float(cfg.single_p)

    multi_rands = gen.random(n_people)
    picked = np.searchsorted(cdf_2plus, multi_rands)
    multi_sizes = np.clip(2 + picked, 2, int(cfg.max_size))

    sizes = np.where(is_single, 1, multi_sizes)

    cum_sizes = np.cumsum(sizes)
    cutoff = int(np.searchsorted(cum_sizes, n_people)) + 1

    return cast(list[int], sizes[:cutoff].tolist())


def build(
    cfg: config.Family,
    gen: np.random.Generator,
    *,
    people: list[str],
) -> Partition:
    """
    Groups individuals into physical households.
    """
    n_people = len(people)
    if n_people == 0:
        return Partition(households=[], household_map={})

    #  Prepare Distributions
    cdf_2plus = _build_size_cdf(
        alpha=float(cfg.zipf_alpha),
        max_size=int(cfg.max_size),
    )

    #  Vectorized Size Sampling
    sizes = _sample_sizes(cfg, gen, cdf_2plus, n_people)

    #  Shuffle the Population
    perm_indices = cast(list[int], gen.permutation(n_people).tolist())
    permuted_people = [people[i] for i in perm_indices]

    #  Chunk into Households
    households: list[list[str]] = []
    household_map: dict[str, int] = {}

    cursor = 0
    for h_id, size in enumerate(sizes):
        end = min(n_people, cursor + size)
        hh = permuted_people[cursor:end]

        households.append(hh)
        for person_id in hh:
            household_map[person_id] = h_id

        cursor = end
        if cursor >= n_people:
            break

    return Partition(
        households=households,
        household_map=household_map,
    )
