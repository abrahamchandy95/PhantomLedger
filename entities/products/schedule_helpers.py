from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime

from common.calendar_cache import WindowCalendar
from common.date_math import clip_half_open

from .event import Direction, ObligationEvent


@dataclass(frozen=True, slots=True)
class MonthlyScheduleSpec:
    active_start: datetime
    active_end_excl: datetime | None
    payment_day: int
    hour: int
    minute: int
    counterparty_acct: str
    amount: float
    channel: str
    product_type: str
    direction: Direction = Direction.OUTFLOW


def scheduled_monthly_events(
    person_id: str,
    spec: MonthlyScheduleSpec,
    start: datetime,
    end_excl: datetime,
    calendar: WindowCalendar | None = None,
) -> Iterator[ObligationEvent]:
    clipped = clip_half_open(
        window_start=start,
        window_end_excl=end_excl,
        active_start=spec.active_start,
        active_end_excl=spec.active_end_excl,
    )
    if clipped is None:
        return

    effective_start, effective_end_excl = clipped

    if calendar is None:
        calendar = WindowCalendar(start, end_excl)

    safe_day = min(max(1, spec.payment_day), 28)

    for ts in calendar.iter_monthly(
        effective_start,
        effective_end_excl,
        day=safe_day,
        hour=spec.hour,
        minute=spec.minute,
    ):
        yield ObligationEvent(
            person_id=person_id,
            direction=spec.direction,
            counterparty_acct=spec.counterparty_acct,
            amount=spec.amount,
            timestamp=ts,
            channel=spec.channel,
            product_type=spec.product_type,
        )
