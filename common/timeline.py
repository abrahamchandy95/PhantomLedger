from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import Final

from common.random import Rng

ISO_FORMAT: Final = "%Y-%m-%d %H:%M:%S"


def format_ts(ts: datetime) -> str:
    """Standardize timestamp formatting for CSV output."""
    return ts.strftime(ISO_FORMAT)


def offset(start: datetime, days: int) -> datetime:
    """Return date at a specific day offset."""
    if days < 0:
        raise ValueError("offset days must be >= 0")
    return start + timedelta(days=days)


def daily(start: datetime, days: int) -> Iterator[datetime]:
    """Yield daily timestamps for a duration."""
    for i in range(max(0, days)):
        yield start + timedelta(days=i)


def months(start: datetime, days: int) -> list[datetime]:
    """Return the first day of every month touched by this range."""
    if days < 0:
        raise ValueError("days must be >= 0")

    end = start + timedelta(days=days)
    curr = datetime(start.year, start.month, 1)
    results: list[datetime] = []

    while curr < end:
        results.append(curr)
        # Advance to first of next month
        next_m = curr.month + 1
        next_y = curr.year
        if next_m > 12:
            next_m = 1
            next_y += 1
        curr = datetime(next_y, next_m, 1)

    return results


def active_months(start: datetime, days: int) -> list[datetime]:
    """Return month starts that fall strictly within [start, start+days]."""
    return [m for m in months(start, days) if m >= start]


def sample_span(rng: Rng, start: datetime, days: int) -> tuple[datetime, datetime]:
    """Sample a (first_seen, last_seen) pair within the window."""
    if days <= 0:
        raise ValueError("days must be > 0")

    first = start + timedelta(days=rng.int(0, days))

    # Calculate remaining days from 'first' to the end of the window
    remaining = days - (first - start).days
    last = first + timedelta(days=rng.int(0, max(1, remaining)))

    return first, last


def sample_short_span(
    rng: Rng, start: datetime, days: int
) -> tuple[datetime, datetime]:
    """Sample a short 2 to 8 day burst within the overall timeframe."""
    span_days = min(days, rng.int(2, 8))
    max_start_offset = max(0, days - span_days)
    start_offset = 0 if max_start_offset == 0 else rng.int(0, max_start_offset + 1)

    first_seen = start + timedelta(days=start_offset)
    last_seen = first_seen + timedelta(days=span_days - 1)
    return first_seen, last_seen


@dataclass(frozen=True, slots=True)
class Period:
    """An active simulation window used for iteration and math."""

    start: datetime
    days: int

    def __post_init__(self) -> None:
        if self.days <= 0:
            raise ValueError("Period duration (days) must be > 0")

    @property
    def end(self) -> datetime:
        """The exclusive end boundary of the period."""
        return self.start + timedelta(days=self.days)

    def daily(self) -> Iterator[datetime]:
        return daily(self.start, self.days)

    def months(self) -> list[datetime]:
        return months(self.start, self.days)
