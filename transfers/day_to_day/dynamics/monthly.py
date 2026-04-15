"""
Monthly counterparty set evolution.
"""

from dataclasses import dataclass
from typing import cast

import numpy as np

from common.math import F64, I16, I32, as_int
from common.random import Rng
from math_models.counterparty_evolution import (
    EvolutionConfig,
    evolve_contacts,
    evolve_favorites,
)


@dataclass(frozen=True, slots=True)
class FavoritesView:
    """Mutable view over merchant favorite arrays (mutated in place)."""

    indices: I32
    counts: I16
    cdf: F64
    total: int


@dataclass(frozen=True, slots=True)
class ContactsView:
    """Mutable view over P2P contact arrays (mutated in place)."""

    matrix: I32
    degree: int
    n_people: int


def evolve_all(
    rng: Rng,
    cfg: EvolutionConfig,
    favorites: FavoritesView,
    contacts: ContactsView,
) -> None:
    """
    Evolve counterparty sets for all people. Called at month boundaries.

    Mutates favorites.indices, favorites.counts, and contacts.matrix
    in place to avoid reallocation.
    """
    max_cols = as_int(cast(int | np.integer, favorites.indices.shape[1]))

    for i in range(contacts.n_people):
        _evolve_person_favorites(rng, cfg, favorites, i, max_cols)
        _evolve_person_contacts(rng, cfg, contacts, i)


def _evolve_person_favorites(
    rng: Rng,
    cfg: EvolutionConfig,
    fav: FavoritesView,
    person_idx: int,
    max_cols: int,
) -> None:
    """Evolve one person's favorite merchant set."""
    current_k = as_int(cast(int | np.integer, fav.counts[person_idx]))
    current_favs = [
        as_int(cast(int | np.integer, fav.indices[person_idx, j]))
        for j in range(current_k)
    ]

    new_favs = evolve_favorites(rng, cfg, current_favs, fav.cdf, fav.total)

    new_k = min(len(new_favs), max_cols)
    fav.counts[person_idx] = np.int16(new_k)

    if new_k > 0:
        fav.indices[person_idx, :] = np.int32(new_favs[0])
        for j in range(new_k):
            fav.indices[person_idx, j] = np.int32(new_favs[j])


def _evolve_person_contacts(
    rng: Rng,
    cfg: EvolutionConfig,
    ctx: ContactsView,
    person_idx: int,
) -> None:
    """Evolve one person's P2P contact row."""
    row: I32 = np.asarray(ctx.matrix[person_idx], dtype=np.int32)
    ctx.matrix[person_idx] = evolve_contacts(
        rng,
        cfg,
        row,
        ctx.degree,
        person_idx,
        ctx.n_people,
    )
