from math import log
from typing import cast, overload

import numpy as np
import numpy.typing as npt


type NumScalar = float | int | np.floating | np.integer
type ArrF64 = npt.NDArray[np.float64]


def as_float(x: NumScalar) -> float:
    return float(x)


def as_int(x: int | np.integer) -> int:
    return int(x)


def as_f64_array(x: object) -> ArrF64:
    return np.asarray(x, dtype=np.float64)


def build_cdf(weights: npt.ArrayLike) -> ArrF64:
    """
    Convert a 1D non-negative weight vector into a cumulative distribution.
    """
    w = np.asarray(weights, dtype=np.float64).reshape(-1)

    if w.size == 0:
        raise ValueError("weights must be non-empty")
    if np.any(w < 0.0):
        raise ValueError("weights must be non-negative")

    s = float(np.sum(w, dtype=np.float64))
    if not np.isfinite(s) or s <= 0.0:
        raise ValueError("weights must sum to a finite positive value")

    cdf = np.cumsum(w / s, dtype=np.float64)
    cdf[-1] = 1.0
    return cdf


def cdf_pick(cdf: ArrF64, u: float) -> int:
    """
    Pick an index from a cumulative distribution using u in [0, 1].
    """
    if cdf.ndim != 1 or cdf.size == 0:
        raise ValueError("cdf must be a non-empty 1D array")

    j = as_int(cast(int | np.integer, np.searchsorted(cdf, float(u), side="right")))
    if j >= int(cdf.size):
        return int(cdf.size) - 1
    return j


@overload
def lognormal_by_median(
    gen: np.random.Generator,
    *,
    median: float,
    sigma: float,
    size: None = None,
) -> float: ...


@overload
def lognormal_by_median(
    gen: np.random.Generator,
    *,
    median: float,
    sigma: float,
    size: int | tuple[int, ...],
) -> ArrF64: ...


def lognormal_by_median(
    gen: np.random.Generator,
    *,
    median: float,
    sigma: float,
    size: int | tuple[int, ...] | None = None,
) -> float | ArrF64:
    """
    Sample a lognormal distribution parameterized by median instead of mu.

    median = exp(mu)  =>  mu = log(median)
    """
    if float(median) <= 0.0:
        raise ValueError("median must be > 0")
    if float(sigma) < 0.0:
        raise ValueError("sigma must be >= 0")

    mu = log(float(median))

    if size is None:
        scalar_obj = cast(object, gen.lognormal(mean=mu, sigma=float(sigma)))
        return as_float(cast(NumScalar, scalar_obj))

    array_obj = cast(object, gen.lognormal(mean=mu, sigma=float(sigma), size=size))
    return np.asarray(array_obj, dtype=np.float64)
