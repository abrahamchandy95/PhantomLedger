from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime, timedelta

from common.random import Rng

DATETIME_FORMAT = "%Y-%m-%d %H:%M:%S"


def format_datetime(timestamp: datetime) -> str:
    return timestamp.strftime(DATETIME_FORMAT)


def get_date_at_offset(start_date: datetime, offset_days: int) -> datetime:
    if offset_days < 0:
        raise ValueError("offset_days must be >= 0")
    return start_date + timedelta(days=offset_days)


def yield_daily_dates(start_date: datetime, num_days: int) -> Iterator[datetime]:
    if num_days < 0:
        raise ValueError("num_days must be >= 0")
    for offset in range(num_days):
        yield start_date + timedelta(days=offset)


def get_overlapping_months(start_date: datetime, num_days: int) -> list[datetime]:
    """
    Return calendar month starts touched by the simulation range.
    Note: May include the first day of the starting month even if earlier than start_date.
    """
    if num_days < 0:
        raise ValueError("num_days must be >= 0")

    end_date = start_date + timedelta(days=num_days)
    current_month = datetime(start_date.year, start_date.month, 1)

    touched_months: list[datetime] = []

    while current_month < end_date:
        touched_months.append(current_month)

        # De-cluttered the clever math into highly readable steps
        year_carry = current_month.month == 12
        next_year = current_month.year + year_carry
        next_month = 1 if year_carry else current_month.month + 1

        current_month = datetime(next_year, next_month, 1)

    return touched_months


def get_active_months(start_date: datetime, num_days: int) -> list[datetime]:
    """
    Return month anchors that fall strictly within the actual simulation window.
    Unlike get_touched_month_starts(), this never yields a date before start_date.
    """
    if num_days < 0:
        raise ValueError("num_days must be >= 0")

    all_touched_months = get_overlapping_months(start_date, num_days)
    return [month for month in all_touched_months if month >= start_date]


def sample_active_dates(
    rng: Rng, start_date: datetime, num_days: int
) -> tuple[datetime, datetime]:
    """
    Sample (first_seen, last_seen) within the simulation window.
    """
    if num_days <= 0:
        raise ValueError("num_days must be > 0")

    first_seen = start_date + timedelta(days=rng.int(0, num_days))

    days_remaining = num_days - (first_seen - start_date).days
    span = max(1, days_remaining)

    last_seen = first_seen + timedelta(days=rng.int(0, span))

    return first_seen, last_seen


@dataclass(frozen=True, slots=True)
class SimulationPeriod:
    start_date: datetime
    num_days: int

    @property
    def end_date_exclusive(self) -> datetime:
        return self.start_date + timedelta(days=self.num_days)

    def validate(self) -> None:
        if self.num_days <= 0:
            raise ValueError("num_days must be > 0")
