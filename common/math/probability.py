from math import log
from typing import overload, cast

import numpy as np
import numpy.typing as npt

from .numeric import F64


def build_cdf(weights: npt.ArrayLike) -> F64:
    """Convert non-negative weights into a cumulative distribution."""
    w = np.asarray(weights, dtype=np.float64).flatten()

    if w.size == 0:
        raise ValueError("Weights cannot be empty")
    if np.any(w < 0.0):
        raise ValueError("Weights must be non-negative")

    total = float(np.sum(w))
    if not np.isfinite(total) or total <= 0.0:
        raise ValueError("Weights must sum to a finite positive value")

    cdf = np.cumsum(w / total)
    cdf[-1] = 1.0
    return cdf


def cdf_pick(cdf: F64, u: float) -> int:
    """
    Pick an index from a CDF using u in [0, 1].
    """
    # searchsorted returns the index where u would be inserted
    idx = int(np.searchsorted(cdf, u, side="right"))
    n = cast(int, cdf.shape[0])
    if idx >= n:
        return n - 1
    return idx


@overload
def lognormal_by_median(
    gen: np.random.Generator, *, median: float, sigma: float, size: None = None
) -> float: ...


@overload
def lognormal_by_median(
    gen: np.random.Generator,
    *,
    median: float,
    sigma: float,
    size: int | tuple[int, ...],
) -> F64: ...


def lognormal_by_median(
    gen: np.random.Generator,
    *,
    median: float,
    sigma: float,
    size: int | tuple[int, ...] | None = None,
) -> float | F64:
    """Sample a lognormal distribution parameterized by median (exp(mu))."""
    if median <= 0.0:
        raise ValueError(f"Median must be > 0, got {median}")
    if sigma < 0.0:
        raise ValueError(f"Sigma must be >= 0, got {sigma}")

    mu = log(median)

    res = gen.lognormal(mean=mu, sigma=sigma, size=size)

    if size is None:
        return float(res)
    return np.asarray(res, dtype=np.float64)
