from collections.abc import Sequence
from typing import cast

import numpy as np

from common.math import as_int


def rand_int(
    rng: np.random.Generator,
    low: int,
    high_exclusive: int,
) -> int:
    return as_int(cast(int | np.integer, rng.integers(low, high_exclusive)))


def pick_one(
    rng: np.random.Generator,
    items: Sequence[str],
) -> str | None:
    if not items:
        return None
    idx = rand_int(rng, 0, len(items))
    return items[idx]


def choice_k(
    rng: np.random.Generator,
    items: Sequence[str],
    *,
    low: int,
    high: int,
) -> tuple[str, ...]:
    if not items:
        return ()

    low = max(1, int(low))
    high = max(low, int(high))
    k = min(rand_int(rng, low, high + 1), len(items))

    pool = list(items)
    out: list[str] = []
    for _ in range(k):
        idx = rand_int(rng, 0, len(pool))
        out.append(pool.pop(idx))

    return tuple(out)


def payment_count(
    rng: np.random.Generator,
    *,
    low: int,
    high: int,
) -> int:
    return rand_int(rng, low, high + 1)
