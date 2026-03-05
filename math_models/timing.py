from dataclasses import dataclass
import typing
import numpy as np
import numpy.typing as npt

from common.rng import Rng


def _normalize(p: npt.NDArray[np.float64]) -> npt.NDArray[np.float64]:
    # typing.cast removes the 'Any' leakage completely at type-checking time
    # without adding overhead at runtime.
    s = float(typing.cast(float, p.sum()))

    if s <= 0.0 or not np.isfinite(s):
        raise ValueError("invalid probability vector")

    return p / s


@dataclass(frozen=True, slots=True)
class TimingProfiles:
    consumer: np.ndarray
    consumer_day: np.ndarray
    business: np.ndarray


def default_timing_profiles() -> TimingProfiles:
    # 24-hour discrete distributions. Shape is "roughly realistic": business hours peak for business,
    # evening peaks for consumers, retirees more daytime.
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
    return TimingProfiles(
        consumer=_normalize(c), consumer_day=_normalize(c_day), business=_normalize(b)
    )


def sample_offsets_seconds(
    rng: Rng,
    profile_name: str,
    n: int,
    profiles: TimingProfiles,
) -> np.ndarray:
    """
    Returns array of offsets in seconds from the day start.
    """
    if n <= 0:
        return np.zeros(0, dtype=np.int32)

    if profile_name == "consumer":
        p = profiles.consumer
    elif profile_name == "consumer_day":
        p = profiles.consumer_day
    elif profile_name == "business":
        p = profiles.business
    else:
        raise ValueError(f"unknown timing profile: {profile_name}")

    hours = rng.gen.choice(24, size=n, p=p)
    minutes = rng.gen.integers(0, 60, size=n)
    seconds = rng.gen.integers(0, 60, size=n)

    return (hours * 3600 + minutes * 60 + seconds).astype(np.int32)
