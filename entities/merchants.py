from typing import cast

import numpy as np

from common import config
from common.ids import merchant_external_id, merchant_id
from common.math import F64
from common.random import Rng
from . import models


def _normalized_weights(raw: np.ndarray) -> F64:
    weights: F64 = np.asarray(raw, dtype=np.float64)

    weights_sum = float(np.sum(weights))
    if not np.isfinite(weights_sum) or weights_sum <= 0.0:
        weights[:] = 1.0
        weights_sum = float(weights.size)

    weights /= weights_sum
    return weights


def build(
    merch_cfg: config.Merchants,
    pop_cfg: config.Population,
    rng: Rng,
) -> models.Merchants:
    """
    Build a merchant universe with:
      - a repeated "core" set
      - a sparse external long tail

    This keeps the downstream model shape unchanged while making the
    counterparty world much less concentrated.
    """
    core_count = int(round(merch_cfg.per_10k_people * (pop_cfg.size / 10_000.0)))
    core_count = max(250, core_count)

    long_tail_count = int(
        round(merch_cfg.long_tail_external_per_10k_people * (pop_cfg.size / 10_000.0))
    )
    long_tail_count = max(0, long_tail_count)

    total_count = core_count + long_tail_count

    categories_pool = list(merch_cfg.categories)

    merchant_ids = [f"MERCH{i:06d}" for i in range(1, total_count + 1)]

    core_categories = cast(
        list[str],
        rng.gen.choice(categories_pool, size=core_count).tolist(),
    )
    tail_categories = cast(
        list[str],
        rng.gen.choice(categories_pool, size=long_tail_count).tolist(),
    )
    categories = core_categories + tail_categories

    core_raw = rng.gen.lognormal(mean=0.0, sigma=merch_cfg.size_sigma, size=core_count)
    core_weights = _normalized_weights(np.asarray(core_raw, dtype=np.float64))

    if long_tail_count > 0:
        tail_raw = rng.gen.lognormal(
            mean=-0.75,
            sigma=merch_cfg.long_tail_size_sigma,
            size=long_tail_count,
        )
        tail_weights = _normalized_weights(np.asarray(tail_raw, dtype=np.float64))
    else:
        tail_weights = np.asarray([], dtype=np.float64)

    tail_share = float(merch_cfg.long_tail_weight_share)
    core_share = 1.0 - tail_share

    if long_tail_count > 0:
        weights: F64 = np.concatenate(
            [
                core_weights * core_share,
                tail_weights * tail_share,
            ]
        )
    else:
        weights = core_weights

    core_is_in_bank = cast(
        list[bool],
        (rng.gen.random(size=core_count) < merch_cfg.in_bank_p).tolist(),
    )

    counterparties: list[str] = []
    internals: list[str] = []
    externals: list[str] = []

    for i, in_bank in enumerate(core_is_in_bank, start=1):
        acct = merchant_id(i) if in_bank else merchant_external_id(i)
        counterparties.append(acct)
        if in_bank:
            internals.append(acct)
        else:
            externals.append(acct)

    for i in range(core_count + 1, total_count + 1):
        acct = merchant_external_id(i)
        counterparties.append(acct)
        externals.append(acct)

    return models.Merchants(
        ids=merchant_ids,
        counterparties=counterparties,
        categories=categories,
        weights=weights,
        internals=internals,
        externals=externals,
    )
