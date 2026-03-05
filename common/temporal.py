from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime, timedelta

from common.rng import Rng


TG_DATETIME_FORMAT = "%Y-%m-%d %H:%M:%S"


def dt_str(dt: datetime) -> str:
    """TigerGraph loader-friendly DATETIME string."""
    return dt.strftime(TG_DATETIME_FORMAT)


def day_start(start_date: datetime, day_index: int) -> datetime:
    if day_index < 0:
        raise ValueError("day_index must be >= 0")
    return start_date + timedelta(days=day_index)


def iter_days(start_date: datetime, days: int) -> Iterator[datetime]:
    if days < 0:
        raise ValueError("days must be >= 0")
    for d in range(days):
        yield start_date + timedelta(days=d)


def iter_month_starts(start_date: datetime, days: int) -> list[datetime]:
    if days < 0:
        raise ValueError("days must be >= 0")

    end = start_date + timedelta(days=days)
    d = datetime(start_date.year, start_date.month, 1)

    out: list[datetime] = []
    while d < end:
        out.append(d)
        d = datetime(d.year + (d.month == 12), 1 if d.month == 12 else d.month + 1, 1)

    return out


def sample_seen_window(
    rng: Rng, start_date: datetime, days: int
) -> tuple[datetime, datetime]:
    """
    Sample (first_seen, last_seen) within the simulation window.

    Mirrors your original logic:
      fs = start + rand_day
      ls = fs + rand(0..remaining_days)
    """
    if days <= 0:
        raise ValueError("days must be > 0")

    fs = start_date + timedelta(days=rng.int(0, days))
    remaining = days - (fs - start_date).days
    span = max(1, remaining)
    ls = fs + timedelta(days=rng.int(0, span))
    return fs, ls


@dataclass(frozen=True, slots=True)
class DateWindow:
    start: datetime
    days: int

    @property
    def end_exclusive(self) -> datetime:
        return self.start + timedelta(days=self.days)

    def validate(self) -> None:
        if self.days <= 0:
            raise ValueError("days must be > 0")
