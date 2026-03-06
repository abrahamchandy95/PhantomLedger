from dataclasses import dataclass
from typing import cast

import numpy as np

from common.rng import Rng
from common.seeding import derived_seed

_NumScalar = float | int | np.floating | np.integer


def _as_int(x: object) -> int:
    return int(cast(int | np.integer, x))


@dataclass(frozen=True, slots=True)
class SocialGraph:
    # For each person index i, contacts[i] gives person indices (int32) of likely P2P counterparties
    contacts: np.ndarray  # shape (n_people, k_contacts) int32
    k_contacts: int


def build_social_graph(
    rng: Rng,
    *,
    seed: int,
    people: list[str],
    k_contacts: int = 8,
    community_min: int = 80,
    community_max: int = 450,
    cross_community_p: float = 0.03,
) -> SocialGraph:
    """
    Community-block model:
      - people are partitioned into contiguous communities (block sizes in [min,max])
      - each person chooses most contacts within their community
      - small probability of cross-community links

    This is intentionally *not* an ER-like random graph, so hop distances do not collapse.
    """
    n = len(people)
    if n == 0:
        return SocialGraph(
            contacts=np.zeros((0, k_contacts), dtype=np.int32), k_contacts=k_contacts
        )

    if k_contacts <= 0:
        raise ValueError("k_contacts must be > 0")

    if community_max < community_min:
        raise ValueError("community_max must be >= community_min")

    # community boundaries as contiguous blocks
    starts: list[int] = []
    ends: list[int] = []

    i = 0
    while i < n:
        # random-ish block size
        size = community_min + rng.int(0, max(1, (community_max - community_min + 1)))
        j = min(n, i + size)
        starts.append(i)
        ends.append(j)
        i = j

    # For each person, find their block by scanning boundaries.
    # Since blocks are contiguous, we can walk once.
    block_for_person = np.empty(n, dtype=np.int32)
    b = 0
    for idx in range(n):
        while idx >= ends[b]:
            b += 1
        block_for_person[idx] = b

    contacts = np.empty((n, k_contacts), dtype=np.int32)

    # Deterministic contact selection per person (stable regardless of loop order)
    for idx, pid in enumerate(people):
        b = _as_int(cast(object, block_for_person[idx]))
        lo = starts[b]
        hi = ends[b]

        g = np.random.default_rng(derived_seed(seed, "p2p_contacts", pid))

        for j in range(k_contacts):
            if float(g.random()) < float(cross_community_p):
                # global contact
                other = int(g.integers(0, n))
            else:
                # within-community
                other = int(g.integers(lo, hi))

            if other == idx:
                other = lo if idx != lo else min(hi - 1, idx + 1)

            contacts[idx, j] = int(other)

    return SocialGraph(contacts=contacts, k_contacts=k_contacts)
