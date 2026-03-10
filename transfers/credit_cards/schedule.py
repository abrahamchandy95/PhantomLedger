import calendar
from datetime import datetime, timedelta


def _safe_month_day(year: int, month: int, day: int) -> int:
    last = calendar.monthrange(year, month)[1]
    return min(day, last)


def iter_cycle_closes(
    start: datetime,
    end_excl: datetime,
    cycle_day: int,
) -> list[datetime]:
    """
    Monthly close timestamps inside [start, end_excl).
    Close time is fixed at 23:30 on the close day.
    """
    out: list[datetime] = []

    year = start.year
    month = start.month

    while True:
        day = _safe_month_day(year, month, cycle_day)
        close = datetime(year, month, day, 23, 30, 0)

        if close < start:
            if month == 12:
                year, month = year + 1, 1
            else:
                month += 1
            continue

        if close >= end_excl:
            break

        out.append(close)

        if month == 12:
            year, month = year + 1, 1
        else:
            month += 1

    return out


def effective_due_cutoff(due: datetime) -> datetime:
    """
    Approximate 'received by due date' logic.

    Weekend part only: if the due date lands on a weekend, move cutoff
    to the next weekday, preserving time-of-day cleanup.
    """
    cutoff = due

    while cutoff.weekday() >= 5:
        cutoff += timedelta(days=1)

    return cutoff.replace(second=0, microsecond=0)


def payment_received_on_time(payment_ts: datetime, due: datetime) -> bool:
    return payment_ts <= effective_due_cutoff(due)
