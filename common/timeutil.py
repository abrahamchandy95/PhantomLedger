from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime, timedelta


TG_DATETIME_FORMAT = "%Y-%m-%d %H:%M:%S"


def dt_str(dt: datetime) -> str:
    """TigerGraph loader-friendly DATETIME string."""
    return dt.strftime(TG_DATETIME_FORMAT)


def day_start(start_date: datetime, day_index: int) -> datetime:
    """Start datetime for a given day index (0-based) in the simulation window."""
    if day_index < 0:
        raise ValueError("day_index must be >= 0")
    return start_date + timedelta(days=day_index)


def iter_days(start_date: datetime, days: int) -> Iterator[datetime]:
    """Yield each day start in the simulation window."""
    if days < 0:
        raise ValueError("days must be >= 0")
    for d in range(days):
        yield start_date + timedelta(days=d)


def iter_month_starts(start_date: datetime, days: int) -> list[datetime]:
    """
    Return month-start datetimes within [start_date, start_date + days).
    Mirrors your original month_iter() logic.
    """
    if days < 0:
        raise ValueError("days must be >= 0")

    end = start_date + timedelta(days=days)
    d = datetime(start_date.year, start_date.month, 1)

    out: list[datetime] = []
    while d < end:
        out.append(d)
        if d.month == 12:
            d = datetime(d.year + 1, 1, 1)
        else:
            d = datetime(d.year, d.month + 1, 1)

    return out


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
