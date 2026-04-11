import calendar
from collections.abc import Iterator
from datetime import datetime, timedelta

from .state import PayCadence, PayrollProfile


def _midnight(ts: datetime) -> datetime:
    return datetime(ts.year, ts.month, ts.day)


def _roll_weekend(ts: datetime, mode: str) -> datetime:
    out = _midnight(ts)
    while out.weekday() >= 5:
        out += timedelta(days=-1 if mode == "previous_business_day" else 1)
    return out


def _month_day(year: int, month: int, day: int) -> datetime:
    last = calendar.monthrange(year, month)[1]
    return datetime(year, month, min(day, last))


def _next_weekday_on_or_after(ts: datetime, weekday: int) -> datetime:
    ts = _midnight(ts)
    delta = (weekday - ts.weekday()) % 7
    return ts + timedelta(days=delta)


def iter_scheduled_paydates(
    profile: PayrollProfile,
    start: datetime,
    end_excl: datetime,
) -> Iterator[datetime]:
    start = _midnight(start)
    end_excl = _midnight(end_excl)

    if end_excl <= start:
        return

    if profile.cadence is PayCadence.WEEKLY:
        current = _next_weekday_on_or_after(
            max(start, profile.anchor_date), profile.weekday
        )
        while current < end_excl:
            yield _roll_weekend(current, profile.weekend_roll)
            current += timedelta(days=7)
        return

    if profile.cadence is PayCadence.BIWEEKLY:
        current = _next_weekday_on_or_after(profile.anchor_date, profile.weekday)
        while current < start:
            current += timedelta(days=14)
        while current < end_excl:
            yield _roll_weekend(current, profile.weekend_roll)
            current += timedelta(days=14)
        return

    if profile.cadence is PayCadence.SEMIMONTHLY:
        cursor = datetime(start.year, start.month, 1)
        while cursor < end_excl:
            for day in profile.semimonthly_days:
                pay_date = _roll_weekend(
                    _month_day(cursor.year, cursor.month, day),
                    profile.weekend_roll,
                )
                if start <= pay_date < end_excl:
                    yield pay_date
            if cursor.month == 12:
                cursor = datetime(cursor.year + 1, 1, 1)
            else:
                cursor = datetime(cursor.year, cursor.month + 1, 1)
        return

    cursor = datetime(start.year, start.month, 1)
    while cursor < end_excl:
        pay_date = _roll_weekend(
            _month_day(cursor.year, cursor.month, profile.monthly_day),
            profile.weekend_roll,
        )
        if start <= pay_date < end_excl:
            yield pay_date
        if cursor.month == 12:
            cursor = datetime(cursor.year + 1, 1, 1)
        else:
            cursor = datetime(cursor.year, cursor.month + 1, 1)


def pay_periods_in_year(profile: PayrollProfile, year: int) -> int:
    start = datetime(year, 1, 1)
    end_excl = datetime(year + 1, 1, 1)
    return max(1, sum(1 for _ in iter_scheduled_paydates(profile, start, end_excl)))
