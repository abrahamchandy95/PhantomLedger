import calendar
from datetime import datetime, timedelta


def _clamp_day(year: int, month: int, desired_day: int) -> int:
    """Clamps the desired day to the actual number of days in the given month."""
    _, last_day = calendar.monthrange(year, month)
    return min(desired_day, last_day)


def calculate_cycle_closes(
    start: datetime,
    end_excl: datetime,
    cycle_day: int,
) -> list[datetime]:
    """
    Calculates all monthly statement closing timestamps within the window.
    Closes are fixed at 23:30 on the cycle day.
    """
    out: list[datetime] = []

    year = start.year
    month = start.month

    while True:
        day = _clamp_day(year, month, cycle_day)
        close = datetime(year, month, day, 23, 30, 0)

        if close >= end_excl:
            break

        if close >= start:
            out.append(close)

        if month == 12:
            year += 1
            month = 1
        else:
            month += 1

    return out


def resolve_due_date(due: datetime) -> datetime:
    """
    Adjusts weekend due dates to the next business day (Monday).
    Strips seconds/microseconds for exact comparisons.
    """
    wd = due.weekday()

    if wd == 5:
        adjusted = due + timedelta(days=2)
    elif wd == 6:
        adjusted = due + timedelta(days=1)
    else:
        adjusted = due

    return adjusted.replace(second=0, microsecond=0)


def is_on_time(payment_ts: datetime, due: datetime) -> bool:
    """Checks if a payment was received before the resolved due date cutoff."""
    return payment_ts <= resolve_due_date(due)
