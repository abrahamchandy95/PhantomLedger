from collections.abc import Callable, Sequence
from datetime import datetime, timedelta
from typing import TYPE_CHECKING, cast

import numpy as np

from common.random import RngFactory
from .state import SeedSource

if TYPE_CHECKING:
    from .policy import Policy

type AnnualRaiseSource = Callable[["Policy", RngFactory, str, int], float]


def build_rng_factory(seed: SeedSource) -> RngFactory:
    """Converts a SeedSource into an active RngFactory."""
    if isinstance(seed, RngFactory):
        return seed
    return RngFactory(int(seed))


def _years_to_days(years: float) -> int:
    return int(round(years * 365.25))


def _anniversaries_passed(start: datetime, now: datetime) -> int:
    years = now.year - start.year
    if (now.month, now.day) < (start.month, start.day):
        years -= 1
    return max(0, years)


def _sample_tenure_days(
    gen: np.random.Generator,
    years_min: float,
    years_max: float,
) -> int:
    if years_max <= years_min:
        return max(1, _years_to_days(years_min))

    years = float(gen.uniform(years_min, years_max))
    return max(1, _years_to_days(years))


#  Public Sampling APIs


def pick_one(gen: np.random.Generator, items: Sequence[str], *, name: str) -> str:
    """Safely picks one item using strict type-unpacked NumPy selection."""
    if not items:
        raise ValueError(f"{name} must be non-empty")

    [idx] = cast(list[int], gen.choice(len(items), size=1).tolist())
    return items[idx]


def pick_different(
    gen: np.random.Generator,
    items: Sequence[str],
    current: str,
    *,
    name: str,
) -> str:
    if not items:
        raise ValueError(f"{name} must be non-empty")
    if len(items) <= 1:
        return current

    # Filter out the current item instantly
    eligible = [x for x in items if x != current]
    if not eligible:
        return current

    [idx] = cast(list[int], gen.choice(len(eligible), size=1).tolist())
    return eligible[idx]


def sample_backdated_interval(
    gen: np.random.Generator,
    *,
    anchor_date: datetime,
    years_min: float,
    years_max: float,
) -> tuple[datetime, datetime]:
    tenure_days = _sample_tenure_days(gen, years_min, years_max)

    # Pick a random day within the tenure for the anchor
    [age_days] = cast(list[int], gen.choice(max(1, tenure_days), size=1).tolist())

    start = anchor_date - timedelta(days=age_days)
    end = start + timedelta(days=tenure_days)
    return start, end


def sample_forward_interval(
    gen: np.random.Generator,
    *,
    start: datetime,
    years_min: float,
    years_max: float,
) -> tuple[datetime, datetime]:
    tenure_days = _sample_tenure_days(gen, years_min, years_max)
    end = start + timedelta(days=tenure_days)
    return start, end


def sample_normal_clamped(
    gen: np.random.Generator,
    *,
    mu: float,
    sigma: float,
    floor: float,
) -> float:
    x = float(gen.normal(loc=mu, scale=sigma))
    return max(floor, x)


def sample_lognormal_multiplier(gen: np.random.Generator, *, sigma: float) -> float:
    return float(gen.lognormal(mean=0.0, sigma=sigma))


def compound_growth(
    *,
    policy: "Policy",
    rng_factory: RngFactory,
    key: str,
    start: datetime,
    now: datetime,
    annual_raise_source: AnnualRaiseSource,
) -> float:
    """Compounds inflation and real wage/rent growth over elapsed years."""
    n = _anniversaries_passed(start, now)
    if n <= 0:
        return 1.0

    growth = 1.0
    for i in range(n):
        year = start.year + i
        real = annual_raise_source(policy, rng_factory, key, year)
        growth *= 1.0 + policy.inflation + real

    return growth
