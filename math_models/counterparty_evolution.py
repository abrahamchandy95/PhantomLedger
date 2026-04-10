"""
Counterparty diversity evolution over time.

Real consumers gradually accumulate and shed counterparties:
a new restaurant discovered this month, a subscription cancelled,
a friend they start splitting costs with. This evolution is slow
and organic — typically 1-3 new merchant relationships per month
with occasional drops.

Vilella et al. (2021, EPJ Data Science 10:25) measured "category
turnover" in bank transaction data as the Jaccard distance between
spending category sets in consecutive time windows. They found
turnover is a stable individual trait correlated with personality
(Openness to Experience), with typical monthly category turnover
of 0.15-0.30 (meaning 15-30% of categories change month to month).

This matters for mule detection because:
- Legitimate accounts show GRADUAL counterparty growth: 1-2 new
  merchants per month, with most activity going to existing favorites.
- Mule accounts show SUDDEN counterparty explosion: 10-25 new
  counterparties appear within days during the burst window
  (FATF 2022, "Money Laundering Through Money Mules").

Without counterparty evolution, all legitimate accounts have a
static merchant set from day 1, which means any new-counterparty
signal trivially flags mules. With evolution, the model must learn
to distinguish the RATE of counterparty acquisition, not just
its presence.

We model evolution as a monthly update to each person's favorite
merchant set and social contacts:
  - With probability p_add, add a new merchant from the global pool
  - With probability p_drop, remove the least-used existing favorite
  - With probability p_new_contact, add a new P2P peer
"""

from dataclasses import dataclass
from typing import cast

import numpy as np

from common.math import I32, cdf_pick, F64
from common.random import Rng
from common.validate import between, ge


@dataclass(frozen=True, slots=True)
class EvolutionConfig:
    """Controls monthly counterparty set evolution."""

    # Monthly probability of adding a new favorite merchant.
    # Vilella et al. 2021: category turnover ~0.15-0.30/month.
    # At merchant level (finer grain), we expect higher churn
    # but most new merchants are one-off, not favorites.
    merchant_add_p: float = 0.35

    # Monthly probability of dropping the least-used favorite.
    merchant_drop_p: float = 0.10

    # Monthly probability of adding a new P2P contact.
    # Social networks grow slowly: ~1-2 new payment contacts/year
    # for typical consumers (estimated from Venmo/Zelle usage data).
    contact_add_p: float = 0.08

    # Monthly probability of a P2P contact going inactive.
    contact_drop_p: float = 0.03

    # Maximum favorites to prevent unbounded growth.
    max_favorites: int = 40

    # Maximum P2P contacts in the active set.
    max_contacts: int = 20

    def __post_init__(self) -> None:
        between("merchant_add_p", self.merchant_add_p, 0.0, 1.0)
        between("merchant_drop_p", self.merchant_drop_p, 0.0, 1.0)
        between("contact_add_p", self.contact_add_p, 0.0, 1.0)
        between("contact_drop_p", self.contact_drop_p, 0.0, 1.0)
        ge("max_favorites", self.max_favorites, 5)
        ge("max_contacts", self.max_contacts, 3)


DEFAULT_EVOLUTION_CONFIG = EvolutionConfig()


def evolve_favorites(
    rng: Rng,
    cfg: EvolutionConfig,
    current_favorites: list[int],
    merch_cdf: F64,
    total_merchants: int,
) -> list[int]:
    """
    Evolve a person's favorite merchant indices for one month.

    Returns a new list of favorite indices (may be same length,
    one longer, or one shorter than input).
    """
    favorites = list(current_favorites)
    existing_set = set(favorites)

    # Possibly add a new merchant
    if rng.coin(float(cfg.merchant_add_p)) and len(favorites) < int(cfg.max_favorites):
        # Cannot add if person already knows every merchant
        if len(existing_set) < total_merchants:
            added = False

            # First try: weighted sampling from the global CDF.
            # Biases toward popular merchants, which is realistic
            # for organic discovery.
            for _ in range(10):
                candidate = cdf_pick(merch_cdf, rng.float())
                if candidate not in existing_set:
                    favorites.append(candidate)
                    added = True
                    break

            # Fallback: uniform sampling over the full merchant pool.
            # Kicks in when popular merchants are already saturated
            # in the favorites set — models discovering a niche shop.
            if not added:
                for _ in range(10):
                    candidate = rng.int(0, total_merchants)
                    if candidate not in existing_set:
                        favorites.append(candidate)
                        break

    # Possibly drop a random favorite (proxy for least-used)
    if rng.coin(float(cfg.merchant_drop_p)) and len(favorites) > 3:
        drop_idx = rng.int(0, len(favorites))
        del favorites[drop_idx]

    return favorites


def evolve_contacts(
    rng: Rng,
    cfg: EvolutionConfig,
    contacts_row: I32,
    degree: int,
    person_idx: int,
    n_people: int,
) -> I32:
    """
    Evolve a person's P2P contact row for one month.

    Modifies up to one slot: either replaces a random slot with
    a new contact (add) or duplicates an existing contact into
    a random slot (drop = reduce effective diversity).

    Returns a new contacts row array.
    """
    row = np.array(contacts_row, dtype=np.int32)

    # Add a new contact: pick a random person not in current set
    if rng.coin(float(cfg.contact_add_p)):
        current_set = set(cast(list[int], row.tolist()))
        for _ in range(10):
            candidate = rng.int(0, n_people)
            if candidate != person_idx and candidate not in current_set:
                # Replace a random slot
                slot = rng.int(0, degree)
                row[slot] = np.int32(candidate)
                break

    # "Drop" a contact by replacing it with a duplicate of another
    # (reduces effective diversity without changing array shape)
    if rng.coin(float(cfg.contact_drop_p)) and degree >= 2:
        drop_slot = rng.int(0, degree)
        keep_slot = rng.int(0, degree)
        if drop_slot != keep_slot:
            row[drop_slot] = row[keep_slot]

    return row
