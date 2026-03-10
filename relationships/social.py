from collections.abc import Callable
from dataclasses import dataclass
from typing import cast

import numpy as np

from common.math import ArrF64, ArrI32, build_cdf, cdf_pick
from common.random import Rng, SeedBank
from common.validate import (
    require_float_between,
    require_float_gt,
    require_int_gt,
)


@dataclass(frozen=True, slots=True)
class SocialGraphPolicy:
    k_contacts: int = 12
    local_contact_p: float = 0.70
    hub_weight_boost: float = 25.0
    attractiveness_sigma: float = 1.1
    edge_weight_gamma_shape: float = 1.0

    def validate(self) -> None:
        require_int_gt("k_contacts", self.k_contacts, 0)
        require_float_between("local_contact_p", self.local_contact_p, 0.0, 1.0)
        require_float_gt("hub_weight_boost", self.hub_weight_boost, 0.0)
        require_float_gt("attractiveness_sigma", self.attractiveness_sigma, 0.0)
        require_float_gt("edge_weight_gamma_shape", self.edge_weight_gamma_shape, 0.0)

    def effective_k_contacts(self) -> int:
        return max(3, min(24, int(self.k_contacts)))

    def cross_community_p(self) -> float:
        return max(0.01, min(0.25, 1.0 - float(self.local_contact_p)))

    def community_bounds(self) -> tuple[int, int]:
        k_contacts = self.effective_k_contacts()
        community_min = max(20, 6 * k_contacts)
        community_max = max(community_min + 20, 24 * k_contacts)
        return community_min, community_max


DEFAULT_SOCIAL_GRAPH_POLICY = SocialGraphPolicy()


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


def _assign_contiguous_communities(
    rng: Rng,
    *,
    n_people: int,
    community_min: int,
    community_max: int,
) -> tuple[list[int], list[int], list[int]]:
    block_starts: list[int] = []
    block_ends: list[int] = []

    cursor = 0
    while cursor < n_people:
        size = community_min + rng.int(0, max(1, community_max - community_min + 1))
        end_idx = min(n_people, cursor + size)
        block_starts.append(cursor)
        block_ends.append(end_idx)
        cursor = end_idx

    block_for_person: list[int] = [0] * n_people
    for block_idx, (lo, hi) in enumerate(zip(block_starts, block_ends)):
        for person_idx in range(lo, hi):
            block_for_person[person_idx] = block_idx

    return block_starts, block_ends, block_for_person


def build_social_graph(
    rng: Rng,
    *,
    seed: int,
    people: list[str],
    policy: SocialGraphPolicy = DEFAULT_SOCIAL_GRAPH_POLICY,
    hub_people: set[str] | None = None,
) -> SocialGraph:
    """
    Person-level contact graph with local communities and weighted tie strength.
    """
    policy.validate()

    n_people = len(people)
    k_contacts = policy.effective_k_contacts()

    if n_people == 0:
        empty = np.zeros((0, k_contacts), dtype=np.int32)
        return SocialGraph(contacts=empty, k_contacts=k_contacts)

    community_min, community_max = policy.community_bounds()
    cross_community_p = policy.cross_community_p()
    local_contact_p = float(policy.local_contact_p)

    block_starts, block_ends, block_for_person = _assign_contiguous_communities(
        rng,
        n_people=n_people,
        community_min=community_min,
        community_max=community_max,
    )

    # -----------------------------
    # attractiveness prior
    # -----------------------------
    attract_obj = cast(
        object,
        rng.gen.lognormal(
            mean=0.0,
            sigma=float(policy.attractiveness_sigma),
            size=n_people,
        ),
    )
    attract: ArrF64 = np.asarray(attract_obj, dtype=np.float64)

    if hub_people:
        people_index = {person_id: i for i, person_id in enumerate(people)}
        boost = float(policy.hub_weight_boost)
        for hub_person in hub_people:
            hub_idx = people_index.get(hub_person)
            if hub_idx is not None:
                attract[hub_idx] *= boost

    global_cdf = build_cdf(attract)

    block_cdfs: list[ArrF64] = []
    for lo, hi in zip(block_starts, block_ends):
        block_cdfs.append(build_cdf(attract[lo:hi]))

    contacts = np.empty((n_people, k_contacts), dtype=np.int32)
    seedbank = SeedBank(seed)

    # -----------------------------
    # per-person contact selection
    # -----------------------------
    for src_idx, person_id in enumerate(people):
        gen = seedbank.generator("p2p_contacts", person_id)

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
                picked_local = cdf_pick(local_cdf, float(gen.random()))
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
                cand = cdf_pick(global_cdf, float(gen.random()))
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
                cand = cdf_pick(global_cdf, float(gen.random()))
                if cand != src_idx and not (local_lo <= cand < local_hi):
                    return cand

                tries += 1
                if tries >= 24:
                    return draw_global()

        base_unique = max(1, min(k_contacts, max(3, (k_contacts + 1) // 2)))

        def draw_candidate() -> int:
            use_local = (local_size > 1) and (float(gen.random()) < local_contact_p)
            if use_local:
                return draw_local()

            force_cross = (n_people - local_size > 0) and (
                float(gen.random()) < cross_community_p
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
            gen.gamma(
                shape=float(policy.edge_weight_gamma_shape),
                scale=1.0,
                size=len(unique_contacts),
            ),
        )
        strength: ArrF64 = np.asarray(strength_obj, dtype=np.float64)
        strength_cdf = build_cdf(strength)

        for contact_slot in range(k_contacts):
            pick = cdf_pick(strength_cdf, float(gen.random()))
            contacts[src_idx, contact_slot] = unique_contacts[pick]

    return SocialGraph(contacts=contacts, k_contacts=k_contacts)
