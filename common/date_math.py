import calendar
from collections.abc import Iterator
from datetime import datetime


def month_start(ts: datetime) -> datetime:
    """Return the first day of ts's month at midnight."""
    return datetime(ts.year, ts.month, 1)


def add_months(ts: datetime, months: int) -> datetime:
    """
    Add calendar months while preserving wall-clock time when possible.

    If the target month has fewer days than ts.day, clamp to the last
    valid day of the target month.
    """
    if months < 0:
        raise ValueError(f"months must be >= 0, got {months}")

    raw_month = (ts.month - 1) + months
    year = ts.year + (raw_month // 12)
    month = (raw_month % 12) + 1
    day = min(ts.day, calendar.monthrange(year, month)[1])

    return ts.replace(year=year, month=month, day=day)


def clip_half_open(
    *,
    window_start: datetime,
    window_end_excl: datetime,
    active_start: datetime,
    active_end_excl: datetime | None,
) -> tuple[datetime, datetime] | None:
    """
    Intersect:
      simulation window [window_start, window_end_excl)
      product window    [active_start, active_end_excl)  (or open-ended)
    """
    if window_end_excl <= window_start:
        return None

    if active_end_excl is not None and active_end_excl <= active_start:
        return None

    start = max(window_start, active_start)
    end = (
        window_end_excl
        if active_end_excl is None
        else min(window_end_excl, active_end_excl)
    )

    if start >= end:
        return None

    return start, end


def month_starts(start: datetime, end_excl: datetime) -> Iterator[datetime]:
    """Yield first-of-month anchors covering [start, end_excl)."""
    current = month_start(start)
    while current < end_excl:
        yield current
        current = add_months(current, 1)
