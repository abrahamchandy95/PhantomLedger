from collections.abc import Callable
from dataclasses import dataclass
from typing import cast

import numpy as np
import numpy.typing as npt

from common.probability import build_cdf, cdf_pick
from common.rng import Rng
from common.seeding import derived_seed


type ArrF64 = npt.NDArray[np.float64]
type ArrI32 = npt.NDArray[np.int32]


@dataclass(frozen=True, slots=True)
class SocialGraph:
    """
    For each person index i, contacts[i] stores person indices (int32) of
    likely P2P counterparties.

    Repeated indices are allowed and intentional: repetition encodes stronger
    ties, because downstream code samples uniformly from this fixed-width row.
    """

    contacts: ArrI32  # shape (n_people, k_contacts)
    k_contacts: int


def _choose_unique_indices(*, needed: int, draw_one: Callable[[], int]) -> list[int]:
    out: list[int] = []
    seen: set[int] = set()

    tries = 0
    max_tries = max(24, 10 * needed)

    while len(out) < needed and tries < max_tries:
        drawn_idx = draw_one()
        tries += 1
        if drawn_idx in seen:
            continue
        seen.add(drawn_idx)
        out.append(drawn_idx)

    return out


def _fallback_other_index(src_idx: int, n_people: int) -> int:
    if n_people <= 1:
        return src_idx
    return (src_idx + 1) % n_people


def build_social_graph(
    rng: Rng,
    *,
    seed: int,
    people: list[str],
    k_contacts: int = 8,
    community_min: int = 80,
    community_max: int = 450,
    cross_community_p: float = 0.03,
    local_contact_p: float = 0.70,
    hub_people: set[str] | None = None,
    hub_weight_boost: float = 1.0,
    attractiveness_sigma: float = 1.0,
    edge_weight_gamma_shape: float = 1.0,
) -> SocialGraph:
    """
    Person-level contact graph with local communities and weighted tie strength.

    Interpretation of parameters:
      - k_contacts: width of stored contact row per person
      - local_contact_p: probability a candidate contact comes from the
        person's local community
      - cross_community_p: probability a non-local draw is forced outside
        the person's community
      - hub_weight_boost: attractiveness multiplier for hub-connected people
      - attractiveness_sigma: dispersion in person attractiveness
      - edge_weight_gamma_shape: concentration of tie strengths among the
        chosen contacts; lower => more uneven / repeated strong ties
    """
    n_people = len(people)
    if n_people == 0:
        empty = np.zeros((0, k_contacts), dtype=np.int32)
        return SocialGraph(contacts=empty, k_contacts=k_contacts)

    if k_contacts <= 0:
        raise ValueError("k_contacts must be > 0")
    if community_min <= 0:
        raise ValueError("community_min must be > 0")
    if community_max < community_min:
        raise ValueError("community_max must be >= community_min")
    if not (0.0 <= cross_community_p <= 1.0):
        raise ValueError("cross_community_p must be between 0 and 1")
    if not (0.0 <= local_contact_p <= 1.0):
        raise ValueError("local_contact_p must be between 0 and 1")
    if hub_weight_boost <= 0.0:
        raise ValueError("hub_weight_boost must be > 0")
    if attractiveness_sigma <= 0.0:
        raise ValueError("attractiveness_sigma must be > 0")
    if edge_weight_gamma_shape <= 0.0:
        raise ValueError("edge_weight_gamma_shape must be > 0")

    # -----------------------------
    # contiguous communities
    # -----------------------------
    block_starts: list[int] = []
    block_ends: list[int] = []

    cursor = 0
    while cursor < n_people:
        size = community_min + rng.int(0, max(1, community_max - community_min + 1))
        end_idx = min(n_people, cursor + size)
        block_starts.append(cursor)
        block_ends.append(end_idx)
        cursor = end_idx

    num_blocks = len(block_starts)

    block_for_person: list[int] = [0] * n_people
    for block_idx in range(num_blocks):
        lo = block_starts[block_idx]
        hi = block_ends[block_idx]
        for person_idx in range(lo, hi):
            block_for_person[person_idx] = block_idx

    # -----------------------------
    # attractiveness prior
    # -----------------------------
    attract_obj = cast(
        object,
        rng.gen.lognormal(mean=0.0, sigma=float(attractiveness_sigma), size=n_people),
    )
    attract: ArrF64 = np.asarray(attract_obj, dtype=np.float64)

    if hub_people:
        people_index = {person_id: i for i, person_id in enumerate(people)}
        boost = float(hub_weight_boost)
        for hub_person in hub_people:
            hub_idx = people_index.get(hub_person)
            if hub_idx is not None:
                attract[hub_idx] *= boost

    global_cdf = build_cdf(attract)

    block_cdfs: list[ArrF64] = []
    for block_idx in range(num_blocks):
        lo = block_starts[block_idx]
        hi = block_ends[block_idx]
        block_cdfs.append(build_cdf(attract[lo:hi]))

    contacts = np.empty((n_people, k_contacts), dtype=np.int32)

    # -----------------------------
    # per-person contact selection
    # -----------------------------
    for src_idx, person_id in enumerate(people):
        g = np.random.default_rng(derived_seed(seed, "p2p_contacts", person_id))

        src_block_idx = block_for_person[src_idx]
        local_lo = block_starts[src_block_idx]
        local_hi = block_ends[src_block_idx]
        local_size = local_hi - local_lo
        local_cdf = block_cdfs[src_block_idx]

        def draw_local() -> int:
            if local_size <= 1:
                return _fallback_other_index(src_idx, n_people)

            tries = 0
            while True:
                picked_local = cdf_pick(local_cdf, float(g.random()))
                cand = local_lo + picked_local
                if cand != src_idx:
                    return cand

                tries += 1
                if tries >= 8:
                    if src_idx != local_lo:
                        return local_lo

                    next_local = src_idx + 1
                    if next_local < local_hi:
                        return next_local

                    return _fallback_other_index(src_idx, n_people)

        def draw_global() -> int:
            if n_people <= 1:
                return src_idx

            tries = 0
            while True:
                cand = cdf_pick(global_cdf, float(g.random()))
                if cand != src_idx:
                    return cand

                tries += 1
                if tries >= 8:
                    return _fallback_other_index(src_idx, n_people)

        def draw_cross_community() -> int:
            if n_people - local_size <= 0:
                return draw_global()

            tries = 0
            while True:
                cand = cdf_pick(global_cdf, float(g.random()))
                if cand != src_idx and not (local_lo <= cand < local_hi):
                    return cand

                tries += 1
                if tries >= 24:
                    return draw_global()

        base_unique = max(1, min(k_contacts, max(3, (k_contacts + 1) // 2)))

        def draw_candidate() -> int:
            use_local = (local_size > 1) and (
                float(g.random()) < float(local_contact_p)
            )
            if use_local:
                return draw_local()

            force_cross = (n_people - local_size > 0) and (
                float(g.random()) < float(cross_community_p)
            )
            if force_cross:
                return draw_cross_community()

            return draw_global()

        unique_contacts = _choose_unique_indices(
            needed=base_unique,
            draw_one=draw_candidate,
        )

        if not unique_contacts:
            unique_contacts = [_fallback_other_index(src_idx, n_people)]

        strength_obj = cast(
            object,
            g.gamma(
                shape=float(edge_weight_gamma_shape),
                scale=1.0,
                size=len(unique_contacts),
            ),
        )
        strength: ArrF64 = np.asarray(strength_obj, dtype=np.float64)
        strength_cdf = build_cdf(strength)

        for contact_slot in range(k_contacts):
            pick = cdf_pick(strength_cdf, float(g.random()))
            contacts[src_idx, contact_slot] = unique_contacts[pick]

    return SocialGraph(contacts=contacts, k_contacts=k_contacts)
