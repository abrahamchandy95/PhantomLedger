from datetime import datetime, timedelta


def is_month_boundary(day_index: int, start_date: datetime) -> bool:
    if day_index <= 0:
        return False

    prev = start_date + timedelta(days=day_index - 1)
    curr = start_date + timedelta(days=day_index)
    return curr.month != prev.month or curr.year != prev.year
