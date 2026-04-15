from dataclasses import dataclass
import typing
import numpy as np
import numpy.typing as npt

from common.random import Rng


def _normalize(p: npt.NDArray[np.float64]) -> npt.NDArray[np.float64]:
    s = float(typing.cast(float, p.sum()))

    if s <= 0.0 or not np.isfinite(s):
        raise ValueError("invalid probability vector")

    return p / s


@dataclass(frozen=True, slots=True)
class Profiles:
    consumer: np.ndarray
    consumer_day: np.ndarray
    business: np.ndarray

    consumer_cdf: np.ndarray
    consumer_day_cdf: np.ndarray
    business_cdf: np.ndarray

    def get(self, name: str) -> np.ndarray:
        """Routes a profile string to the correct probability array."""
        if name == "consumer":
            return self.consumer
        if name == "consumer_day":
            return self.consumer_day
        if name == "business":
            return self.business
        raise ValueError(f"unknown timing profile: {name}")

    def get_cdf(self, name: str) -> np.ndarray:
        """Routes to pre-computed CDF for faster searchsorted sampling."""
        if name == "consumer":
            return self.consumer_cdf
        if name == "consumer_day":
            return self.consumer_day_cdf
        if name == "business":
            return self.business_cdf
        raise ValueError(f"unknown timing profile: {name}")


def _make_cdf(p: np.ndarray) -> np.ndarray:
    cdf = np.cumsum(p)
    cdf[-1] = 1.0
    return cdf


def _build_defaults() -> Profiles:
    """Internal factory to build and normalize the defaults once."""
    c = np.array(
        [
            0.02,
            0.01,
            0.01,
            0.01,
            0.01,
            0.02,
            0.04,
            0.06,
            0.06,
            0.05,
            0.05,
            0.05,
            0.05,
            0.05,
            0.05,
            0.06,
            0.07,
            0.08,
            0.07,
            0.06,
            0.05,
            0.04,
            0.03,
            0.02,
        ],
        dtype=np.float64,
    )
    c_day = np.array(
        [
            0.01,
            0.01,
            0.01,
            0.01,
            0.01,
            0.02,
            0.04,
            0.07,
            0.08,
            0.08,
            0.07,
            0.06,
            0.06,
            0.06,
            0.06,
            0.06,
            0.06,
            0.05,
            0.04,
            0.03,
            0.02,
            0.02,
            0.01,
            0.01,
        ],
        dtype=np.float64,
    )
    b = np.array(
        [
            0.005,
            0.003,
            0.003,
            0.003,
            0.004,
            0.01,
            0.03,
            0.06,
            0.08,
            0.09,
            0.09,
            0.08,
            0.07,
            0.06,
            0.06,
            0.06,
            0.06,
            0.05,
            0.04,
            0.02,
            0.01,
            0.008,
            0.006,
            0.005,
        ],
        dtype=np.float64,
    )
    cn = _normalize(c)
    cdn = _normalize(c_day)
    bn = _normalize(b)

    return Profiles(
        consumer=cn,
        consumer_day=cdn,
        business=bn,
        consumer_cdf=_make_cdf(cn),
        consumer_day_cdf=_make_cdf(cdn),
        business_cdf=_make_cdf(bn),
    )


DEFAULT_PROFILES = _build_defaults()


def sample_offsets(
    rng: Rng,
    profile_name: str,
    n: int,
    profiles: Profiles = DEFAULT_PROFILES,
) -> np.ndarray:
    """
    Returns array of offsets in seconds from the day start.
    """
    if n <= 0:
        return np.zeros(0, dtype=np.int32)

    p = profiles.get(profile_name)

    hours = rng.gen.choice(24, size=n, p=p)
    minutes = rng.gen.integers(0, 60, size=n)
    seconds = rng.gen.integers(0, 60, size=n)

    return (hours * 3600 + minutes * 60 + seconds).astype(np.int32)


def sample_offset(
    rng: Rng,
    profile_name: str,
    profiles: Profiles = DEFAULT_PROFILES,
) -> int:
    """
    Scalar version for the hot transaction-generation loop.

    Uses pre-computed CDF with searchsorted
    """
    cdf = profiles.get_cdf(profile_name)

    u = float(rng.gen.random())
    hour = int(np.searchsorted(cdf, u))
    if hour >= 24:
        hour = 23

    minute = int(rng.gen.integers(0, 60))
    second = int(rng.gen.integers(0, 60))

    return hour * 3600 + minute * 60 + second


def sample_offsets_batch(
    rng: Rng,
    profile_name: str,
    n: int,
    profiles: Profiles = DEFAULT_PROFILES,
) -> np.ndarray:
    """
    Batch version that samples n offsets at once using
    vectorized searchsorted, much faster than n individual calls.
    """
    if n <= 0:
        return np.zeros(0, dtype=np.int32)

    cdf = profiles.get_cdf(profile_name)

    u = rng.gen.random(n)
    hours = np.searchsorted(cdf, u).astype(np.int32)
    hours = np.minimum(hours, 23)

    minutes = rng.gen.integers(0, 60, size=n, dtype=np.int32)
    seconds = rng.gen.integers(0, 60, size=n, dtype=np.int32)

    return hours * 3600 + minutes * 60 + seconds
