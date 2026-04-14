from collections.abc import Callable
from dataclasses import dataclass
from typing import cast

import numpy as np

from common import config
from common.math import F64, I32, build_cdf, cdf_pick
from common.progress import maybe_tqdm
from common.random import Rng, RngFactory

DEFAULT_CONFIG = config.Social()


@dataclass(frozen=True, slots=True)
class Graph:
    """
    For each person index i, contacts[i] stores person indices (int32) of
    likely P2P counterparties.

    Repeated indices are allowed and intentional: repetition encodes stronger
    ties, because downstream code samples uniformly from this fixed-width row.
    """

    contacts: I32
    degree: int


def _choose_unique(*, needed: int, draw_one: Callable[[], int]) -> list[int]:
    out: list[int] = []
    seen: set[int] = set()
    tries, max_tries = 0, max(24, 10 * needed)

    while len(out) < needed and tries < max_tries:
        drawn_idx = draw_one()
        tries += 1
        if drawn_idx not in seen:
            seen.add(drawn_idx)
            out.append(drawn_idx)

    return out


def _fallback_other(src_idx: int, n_people: int) -> int:
    if n_people <= 1:
        return src_idx
    return (src_idx + 1) % n_people


def _assign_communities(
    rng: Rng,
    *,
    n_people: int,
    c_min: int,
    c_max: int,
) -> tuple[list[int], list[int], list[int]]:
    """Partitions the population into contiguous neighborhood blocks."""
    starts: list[int] = []
    ends: list[int] = []
    member_map: list[int] = [0] * n_people

    cursor = 0

    while cursor < n_people:
        size = c_min + rng.int(0, max(1, c_max - c_min + 1))
        end_idx = min(n_people, cursor + size)

        starts.append(cursor)
        ends.append(end_idx)

        block_idx = len(starts) - 1
        for i in range(cursor, end_idx):
            member_map[i] = block_idx

        cursor = end_idx

    return starts, ends, member_map


def _build_social_capital(
    rng: Rng,
    n_people: int,
    people: list[str],
    cfg: config.Social,
    hub_people: set[str] | None,
) -> F64:
    """Calculates base attractiveness (lognormal) and applies hub multipliers."""
    attract_obj = cast(
        object,
        rng.gen.lognormal(
            mean=0.0,
            sigma=float(cfg.influence_sigma),
            size=n_people,
        ),
    )
    attract: F64 = np.asarray(attract_obj, dtype=np.float64)

    if hub_people:
        people_index = {person_id: i for i, person_id in enumerate(people)}
        boost = float(cfg.hub_multiplier)
        for hub_person in hub_people:
            if (hub_idx := people_index.get(hub_person)) is not None:
                attract[hub_idx] *= boost

    return attract


class ContactSampler:
    """Encapsulates CDF generation and the logic for drawing random contacts."""

    # Class-level annotations strictly required by basedpyright
    n_people: int
    cfg: config.Social
    starts: list[int]
    ends: list[int]
    member_map: list[int]
    global_cdf: F64
    block_cdfs: list[F64]

    def __init__(
        self,
        n_people: int,
        cfg: config.Social,
        starts: list[int],
        ends: list[int],
        member_map: list[int],
        attract: F64,
    ):
        self.n_people = n_people
        self.cfg = cfg
        self.starts = starts
        self.ends = ends
        self.member_map = member_map

        self.global_cdf = build_cdf(attract)
        self.block_cdfs = [
            build_cdf(attract[lo:hi]) for lo, hi in zip(starts, ends, strict=True)
        ]

    def draw_unique(self, src_idx: int, prng: Rng) -> list[int]:
        b_idx = self.member_map[src_idx]
        lo, hi = self.starts[b_idx], self.ends[b_idx]
        b_size, b_cdf = hi - lo, self.block_cdfs[b_idx]

        def draw_local() -> int:
            if b_size <= 1:
                return _fallback_other(src_idx, self.n_people)
            tries = 0
            while True:
                cand = lo + cdf_pick(b_cdf, prng.float())
                # Unrolled to satisfy Ruff E701
                if cand != src_idx:
                    return cand

                tries += 1
                if tries >= 8:
                    if src_idx != lo:
                        return lo
                    if src_idx + 1 < hi:
                        return src_idx + 1
                    return _fallback_other(src_idx, self.n_people)

        def draw_global() -> int:
            if self.n_people <= 1:
                return src_idx
            tries = 0
            while True:
                cand = cdf_pick(self.global_cdf, prng.float())
                # Unrolled to satisfy Ruff E701
                if cand != src_idx:
                    return cand

                tries += 1
                if tries >= 8:
                    return _fallback_other(src_idx, self.n_people)

        def draw_cross() -> int:
            if self.n_people - b_size <= 0:
                return draw_global()
            tries = 0
            while True:
                cand = cdf_pick(self.global_cdf, prng.float())
                # Unrolled to satisfy Ruff E701
                if cand != src_idx and not (lo <= cand < hi):
                    return cand

                tries += 1
                if tries >= 24:
                    return draw_global()

        local_p = float(self.cfg.local_prob)
        cross_p = self.cfg.cross_prob

        def draw_candidate() -> int:
            if b_size > 1 and prng.float() < local_p:
                return draw_local()
            if self.n_people - b_size > 0 and prng.float() < cross_p:
                return draw_cross()
            return draw_global()

        degree = self.cfg.effective_degree
        needed = max(1, min(degree, max(3, (degree + 1) // 2)))

        uniques = _choose_unique(needed=needed, draw_one=draw_candidate)
        if not uniques:
            return [_fallback_other(src_idx, self.n_people)]

        return uniques


# --- Orchestrator ---


def build(
    rng: Rng,
    *,
    seed: int,
    people: list[str],
    cfg: config.Social = DEFAULT_CONFIG,
    hub_people: set[str] | None = None,
) -> Graph:
    """
    Orchestrates the generation of a person-to-person contact graph.
    """
    n_people = len(people)
    degree = cfg.effective_degree

    if n_people == 0:
        return Graph(contacts=np.zeros((0, degree), dtype=np.int32), degree=degree)

    #  Partition communities
    c_min, c_max = cfg.community_bounds()
    starts, ends, member_map = _assign_communities(
        rng, n_people=n_people, c_min=c_min, c_max=c_max
    )

    # Assign Social Capital
    attract = _build_social_capital(rng, n_people, people, cfg, hub_people)

    # Initialize the Sampler
    sampler = ContactSampler(n_people, cfg, starts, ends, member_map, attract)

    # Generate the Contact Matrix
    contacts = np.empty((n_people, degree), dtype=np.int32)
    rng_factory = RngFactory(seed)

    for src_idx, person_id in enumerate(
        maybe_tqdm(people, desc="social graph", unit="person", miniters=2_000)
    ):
        prng = rng_factory.rng("p2p_contacts", person_id)

        # Draw base unique contacts
        uniques = sampler.draw_unique(src_idx, prng)

        # Distribute tie strength via Gamma distribution
        strength_obj = cast(
            object,
            prng.gen.gamma(
                shape=float(cfg.tie_strength_shape),
                scale=1.0,
                size=len(uniques),
            ),
        )
        strength_cdf = build_cdf(np.asarray(strength_obj, dtype=np.float64))

        # Fill the slots, allowing repeats based on tie strength
        for slot in range(degree):
            pick = cdf_pick(strength_cdf, prng.float())
            contacts[src_idx, slot] = uniques[pick]

    return Graph(contacts=contacts, degree=degree)
