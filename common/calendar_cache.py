from __future__ import annotations

import calendar as _calendar
from bisect import bisect_left
from dataclasses import dataclass, field
from datetime import date, datetime, timedelta
from typing import Final

from common.date_math import month_starts

_QUARTERLY_DATES: Final[tuple[tuple[int, int], ...]] = (
    (1, 15),  # Q4 prior year
    (4, 15),  # Q1
    (6, 15),  # Q2
    (9, 15),  # Q3
)


def ssa_bucket_for_birth_day(birth_day: int) -> int:
    if birth_day <= 10:
        return 0
    if birth_day <= 20:
        return 1
    return 2


def _nth_weekday_of_month(
    year: int,
    month: int,
    *,
    weekday: int,
    occurrence: int,
) -> date:
    first = date(year, month, 1)
    delta = (weekday - first.weekday()) % 7
    candidate = first + timedelta(days=delta + 7 * (occurrence - 1))

    if candidate.month != month:
        raise ValueError(
            f"{occurrence} occurrence of weekday {weekday} does not exist in "
            + f"{year:04d}-{month:02d}"
        )

    return candidate


def _last_weekday_of_month(year: int, month: int, *, weekday: int) -> date:
    last_dom = _calendar.monthrange(year, month)[1]
    last = date(year, month, last_dom)
    delta = (last.weekday() - weekday) % 7
    return last - timedelta(days=delta)


def _observed_fixed_holiday(year: int, month: int, day: int) -> date:
    actual = date(year, month, day)
    if actual.weekday() == 5:
        return actual - timedelta(days=1)
    if actual.weekday() == 6:
        return actual + timedelta(days=1)
    return actual


def _us_federal_holidays(year: int) -> set[date]:
    return {
        _observed_fixed_holiday(year, 1, 1),  # New Year's Day
        _nth_weekday_of_month(year, 1, weekday=0, occurrence=3),  # MLK Day
        _nth_weekday_of_month(
            year,
            2,
            weekday=0,
            occurrence=3,
        ),  # Washington's Birthday
        _last_weekday_of_month(year, 5, weekday=0),  # Memorial Day
        _observed_fixed_holiday(year, 6, 19),  # Juneteenth
        _observed_fixed_holiday(year, 7, 4),  # Independence Day
        _nth_weekday_of_month(year, 9, weekday=0, occurrence=1),  # Labor Day
        _nth_weekday_of_month(year, 10, weekday=0, occurrence=2),  # Columbus Day
        _observed_fixed_holiday(year, 11, 11),  # Veterans Day
        _nth_weekday_of_month(year, 11, weekday=3, occurrence=4),  # Thanksgiving
        _observed_fixed_holiday(year, 12, 25),  # Christmas Day
    }


def _is_business_day(ts: date, holiday_cache: dict[int, set[date]]) -> bool:
    if ts.weekday() >= 5:
        return False

    holidays = holiday_cache.setdefault(ts.year, _us_federal_holidays(ts.year))
    return ts not in holidays


def _ssa_cycle_payment_date(
    year: int,
    month: int,
    *,
    birth_day: int,
    holiday_cache: dict[int, set[date]] | None = None,
) -> date:
    """
    Standard SSA/RSDI cohorting for post-1997 cycle payments.

    Official rule:
    - 1st-10th  -> second Wednesday
    - 11th-20th -> third Wednesday
    - 21st-31st -> fourth Wednesday

    If the scheduled Wednesday is a federal holiday, SSA pays on the preceding
    day that is not a federal holiday. We also keep the result on a business day.
    """
    if holiday_cache is None:
        holiday_cache = {}

    if birth_day <= 10:
        occurrence = 2
    elif birth_day <= 20:
        occurrence = 3
    else:
        occurrence = 4

    pay_date = _nth_weekday_of_month(year, month, weekday=2, occurrence=occurrence)

    while not _is_business_day(pay_date, holiday_cache):
        pay_date -= timedelta(days=1)

    return pay_date


@dataclass(slots=True)
class WindowCalendar:
    start: datetime
    end_excl: datetime

    month_anchors: tuple[datetime, ...] = field(init=False)

    _monthly_cache: dict[tuple[int, int, int, int], tuple[datetime, ...]] = field(
        default_factory=dict,
        init=False,
        repr=False,
    )
    _annual_cache: dict[tuple[int, int, int, int, int], tuple[datetime, ...]] = field(
        default_factory=dict,
        init=False,
        repr=False,
    )
    _quarterly_cache: tuple[datetime, ...] | None = field(
        default=None,
        init=False,
        repr=False,
    )
    _ssa_cache: dict[int, tuple[date, ...]] = field(
        default_factory=dict,
        init=False,
        repr=False,
    )
    _holiday_cache: dict[int, set[date]] = field(
        default_factory=dict,
        init=False,
        repr=False,
    )

    def __post_init__(self) -> None:
        if self.end_excl < self.start:
            raise ValueError("end_excl must be >= start")

        self.month_anchors = tuple(month_starts(self.start, self.end_excl))

    def _slice_datetimes(
        self,
        items: tuple[datetime, ...],
        active_start: datetime,
        active_end_excl: datetime,
    ) -> tuple[datetime, ...]:
        lo = bisect_left(items, active_start)
        hi = bisect_left(items, active_end_excl)
        return items[lo:hi]

    def iter_monthly(
        self,
        active_start: datetime,
        active_end_excl: datetime,
        *,
        day: int,
        hour: int,
        minute: int,
        second: int = 0,
    ) -> tuple[datetime, ...]:
        if not 1 <= day <= 31:
            raise ValueError(f"day must be in [1, 31], got {day}")

        key = (day, hour, minute, second)
        items = self._monthly_cache.get(key)

        if items is None:
            built: list[datetime] = []

            for month_start in self.month_anchors:
                dom = min(
                    day,
                    _calendar.monthrange(month_start.year, month_start.month)[1],
                )
                ts = month_start.replace(
                    day=dom,
                    hour=hour,
                    minute=minute,
                    second=second,
                    microsecond=0,
                )
                if self.start <= ts < self.end_excl:
                    built.append(ts)

            items = tuple(built)
            self._monthly_cache[key] = items

        return self._slice_datetimes(items, active_start, active_end_excl)

    def iter_annual_fixed(
        self,
        active_start: datetime,
        active_end_excl: datetime,
        *,
        month: int,
        day: int,
        hour: int,
        minute: int,
        second: int = 0,
    ) -> tuple[datetime, ...]:
        if not 1 <= month <= 12:
            raise ValueError(f"month must be in [1, 12], got {month}")
        if not 1 <= day <= 31:
            raise ValueError(f"day must be in [1, 31], got {day}")

        key = (month, day, hour, minute, second)
        items = self._annual_cache.get(key)

        if items is None:
            built: list[datetime] = []

            for year in range(self.start.year, self.end_excl.year + 1):
                dom = min(day, _calendar.monthrange(year, month)[1])
                ts = datetime(year, month, dom, hour, minute, second)
                if self.start <= ts < self.end_excl:
                    built.append(ts)

            items = tuple(built)
            self._annual_cache[key] = items

        return self._slice_datetimes(items, active_start, active_end_excl)

    def iter_estimated_tax(
        self,
        active_start: datetime,
        active_end_excl: datetime,
    ) -> tuple[datetime, ...]:
        items = self._quarterly_cache

        if items is None:
            built: list[datetime] = []

            for year in range(self.start.year, self.end_excl.year + 1):
                for month, day in _QUARTERLY_DATES:
                    ts = datetime(year, month, day, 10, 0, 0)
                    if self.start <= ts < self.end_excl:
                        built.append(ts)

            items = tuple(built)
            self._quarterly_cache = items

        return self._slice_datetimes(items, active_start, active_end_excl)

    def ssa_payment_dates_for_bucket(self, bucket: int) -> tuple[date, ...]:
        """
        Precompute SSA payment dates for one cohort across the window.

        bucket:
            0 -> birth day 1..10
            1 -> birth day 11..20
            2 -> birth day 21..31
        """
        if bucket not in (0, 1, 2):
            raise ValueError(f"bucket must be 0, 1, or 2, got {bucket}")

        items = self._ssa_cache.get(bucket)
        if items is not None:
            return items

        representative_birth_day = (1, 11, 21)[bucket]
        built: list[date] = []

        for month_start in self.month_anchors:
            built.append(
                _ssa_cycle_payment_date(
                    month_start.year,
                    month_start.month,
                    birth_day=representative_birth_day,
                    holiday_cache=self._holiday_cache,
                )
            )

        items = tuple(built)
        self._ssa_cache[bucket] = items
        return items

    def ssa_payment_dates(self, birth_day: int) -> tuple[date, ...]:
        return self.ssa_payment_dates_for_bucket(ssa_bucket_for_birth_day(birth_day))
