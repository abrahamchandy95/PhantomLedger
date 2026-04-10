from typing import cast

import numpy as np

from common import config
from common.ids import merchant_external_id, merchant_id
from common.math import F64
from common.random import Rng
from . import models


def build(
    merch_cfg: config.Merchants,
    pop_cfg: config.Population,
    rng: Rng,
) -> models.Merchants:
    """
    Builds the synthetic merchant population and their payment counterparty accounts.
    """
    num_merchants = int(round(merch_cfg.per_10k_people * (pop_cfg.size / 10_000.0)))
    num_merchants = max(50, num_merchants)

    raw_weights = rng.gen.lognormal(
        mean=0.0, sigma=merch_cfg.size_sigma, size=num_merchants
    )
    weights: F64 = np.asarray(raw_weights, dtype=np.float64)

    weights_sum = float(np.sum(weights))
    if not np.isfinite(weights_sum) or weights_sum <= 0.0:
        weights[:] = 1.0
        weights_sum = float(num_merchants)

    weights /= weights_sum

    # Use categories from config — single source of truth
    categories_pool = list(merch_cfg.categories)

    merchant_ids = [f"MERCH{i:06d}" for i in range(1, num_merchants + 1)]

    categories = cast(
        list[str], rng.gen.choice(categories_pool, size=num_merchants).tolist()
    )

    is_in_bank = cast(
        list[bool], (rng.gen.random(size=num_merchants) < merch_cfg.in_bank_p).tolist()
    )

    counterparties = [
        merchant_id(i) if in_bank else merchant_external_id(i)
        for i, in_bank in enumerate(is_in_bank, start=1)
    ]

    internals = [
        acct
        for acct, in_bank in zip(counterparties, is_in_bank, strict=True)
        if in_bank
    ]
    externals = [
        acct
        for acct, in_bank in zip(counterparties, is_in_bank, strict=True)
        if not in_bank
    ]
    return models.Merchants(
        ids=merchant_ids,
        counterparties=counterparties,
        categories=categories,
        weights=weights,
        internals=internals,
        externals=externals,
    )
