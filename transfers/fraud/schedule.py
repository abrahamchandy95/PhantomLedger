from datetime import datetime, timedelta

from common.random import Rng


def target_illicit_count(*, target_ratio: float, legit_count: int) -> int:
    if target_ratio <= 0.0 or legit_count <= 0:
        return 0
    denom = max(1e-12, 1.0 - float(target_ratio))
    return max(0, int(round((float(target_ratio) * float(legit_count)) / denom)))


def sample_burst_window(
    rng: Rng,
    *,
    start_date: datetime,
    days: int,
    base_tail_days: int,
    burst_min: int,
    burst_max_exclusive: int,
) -> tuple[datetime, int]:
    base = start_date + timedelta(days=rng.int(0, max(1, days - base_tail_days)))
    burst_days = rng.int(burst_min, burst_max_exclusive)
    return base, burst_days


def random_ts(
    rng: Rng,
    *,
    base: datetime,
    day_hi_exclusive: int,
    hour_lo: int,
    hour_hi_exclusive: int,
) -> datetime:
    return base + timedelta(
        days=rng.int(0, max(1, day_hi_exclusive)),
        hours=rng.int(hour_lo, hour_hi_exclusive),
        minutes=rng.int(0, 60),
    )
