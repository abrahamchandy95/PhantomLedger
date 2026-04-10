from datetime import datetime, timedelta

from common.random import Rng


def calculate_illicit_budget(*, target_ratio: float, legit_count: int) -> int:
    """Calculates the total number of illicit transactions needed to achieve the target ratio."""
    if target_ratio <= 0.0 or legit_count <= 0:
        return 0

    denom = max(1e-12, 1.0 - float(target_ratio))
    return max(0, int(round((float(target_ratio) * float(legit_count)) / denom)))


def sample_burst_window(
    rng: Rng,
    *,
    start_date: datetime,
    total_days: int,
    tail_padding_days: int,
    min_burst_days: int,
    max_burst_days: int,
) -> tuple[datetime, int]:
    """Samples a random starting date and duration for a concentrated burst of activity."""
    max_start_offset = max(1, total_days - tail_padding_days)
    base_date = start_date + timedelta(days=rng.int(0, max_start_offset))
    burst_duration = rng.int(min_burst_days, max_burst_days)

    return base_date, burst_duration


def sample_timestamp(
    rng: Rng,
    *,
    base_date: datetime,
    max_days_offset: int,
    min_hour: int,
    max_hour: int,
) -> datetime:
    """Generates a random timestamp within a specific day and hour range."""
    return base_date + timedelta(
        days=rng.int(0, max(1, max_days_offset)),
        hours=rng.int(min_hour, max_hour),
        minutes=rng.int(0, 60),
    )
