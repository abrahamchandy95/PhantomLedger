from datetime import datetime, timedelta

import numpy as np

from .draw import rand_int


def business_day_ts(
    month_start: datetime,
    rng: np.random.Generator,
    *,
    earliest_hour: int,
    latest_hour: int,
    start_day: int = 0,
    end_day_exclusive: int = 28,
) -> datetime:
    start_day = max(0, min(27, int(start_day)))
    end_day_exclusive = max(start_day + 1, min(28, int(end_day_exclusive)))

    for _ in range(16):
        day = rand_int(rng, start_day, end_day_exclusive)
        ts = month_start + timedelta(
            days=day,
            hours=rand_int(rng, earliest_hour, latest_hour + 1),
            minutes=rand_int(rng, 0, 60),
        )
        if ts.weekday() < 5:
            return ts

    ts = month_start + timedelta(days=rand_int(rng, start_day, end_day_exclusive))
    while ts.weekday() >= 5:
        ts += timedelta(days=1)

    return ts.replace(
        hour=earliest_hour,
        minute=rand_int(rng, 0, 60),
        second=0,
        microsecond=0,
    )
