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

    def get(self, name: str) -> np.ndarray:
        """Routes a profile string to the correct probability array."""
        if name == "consumer":
            return self.consumer
        if name == "consumer_day":
            return self.consumer_day
        if name == "business":
            return self.business
        raise ValueError(f"unknown timing profile: {name}")


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
    return Profiles(
        consumer=_normalize(c), consumer_day=_normalize(c_day), business=_normalize(b)
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
    Avoids allocating three 1-element numpy arrays for every sampled offset.
    """
    p = profiles.get(profile_name)

    hour = int(rng.gen.choice(24, p=p))
    minute = int(rng.gen.integers(0, 60))
    second = int(rng.gen.integers(0, 60))

    return hour * 3600 + minute * 60 + second
