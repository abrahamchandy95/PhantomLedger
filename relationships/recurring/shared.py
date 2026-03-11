from collections.abc import Callable, Sequence
from datetime import datetime, timedelta
from typing import TYPE_CHECKING

import numpy as np

from common.random import Rng, SeedBank

from .types import SeedSource

if TYPE_CHECKING:
    from .policy import RecurringPolicy

type AnnualRaiseSampler = Callable[["RecurringPolicy", SeedBank, str, int], float]


def as_seedbank(seed: SeedSource) -> SeedBank:
    if isinstance(seed, SeedBank):
        return seed
    return SeedBank(int(seed))


def years_to_days(years: float) -> int:
    return int(round(years * 365.25))


def draw_index(gen: np.random.Generator, hi: int) -> int:
    if hi <= 0:
        raise ValueError("upper bound must be > 0")
    return int(gen.integers(0, hi))


def pick_one(gen: np.random.Generator, items: Sequence[str], *, name: str) -> str:
    if not items:
        raise ValueError(f"{name} must be non-empty")
    return items[draw_index(gen, len(items))]


def pick_different_or_same(
    rng: Rng,
    items: Sequence[str],
    current: str,
    *,
    name: str,
) -> str:
    if not items:
        raise ValueError(f"{name} must be non-empty")
    if len(items) <= 1:
        return current

    chosen = current
    while chosen == current:
        chosen = rng.choice(items)
    return chosen


def sample_tenure_days(
    gen: np.random.Generator,
    years_min: float,
    years_max: float,
) -> int:
    if years_max <= years_min:
        return max(1, years_to_days(years_min))

    years = float(gen.uniform(years_min, years_max))
    return max(1, years_to_days(years))


def sample_backdated_interval(
    gen: np.random.Generator,
    *,
    anchor_date: datetime,
    years_min: float,
    years_max: float,
) -> tuple[datetime, datetime]:
    tenure_days = sample_tenure_days(gen, years_min, years_max)
    age_days = draw_index(gen, max(1, tenure_days))

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
    tenure_days = sample_tenure_days(gen, years_min, years_max)
    end = start + timedelta(days=tenure_days)
    return start, end


def anniversaries_passed(start: datetime, now: datetime) -> int:
    years = now.year - start.year
    if (now.month, now.day) < (start.month, start.day):
        years -= 1
    return max(0, years)


def normal_clamped(
    gen: np.random.Generator,
    *,
    mu: float,
    sigma: float,
    floor: float,
) -> float:
    x = float(gen.normal(loc=mu, scale=sigma))
    return max(floor, x)


def lognormal_multiplier(gen: np.random.Generator, *, sigma: float) -> float:
    return float(gen.lognormal(mean=0.0, sigma=sigma))


def compound_growth(
    *,
    policy: "RecurringPolicy",
    seedbank: SeedBank,
    key: str,
    start: datetime,
    now: datetime,
    annual_real_raise: AnnualRaiseSampler,
) -> float:
    n = anniversaries_passed(start, now)
    if n <= 0:
        return 1.0

    growth = 1.0
    for i in range(n):
        year = start.year + i
        real = annual_real_raise(policy, seedbank, key, year)
        growth *= 1.0 + policy.annual_inflation_rate + real

    return growth
