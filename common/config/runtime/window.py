from dataclasses import dataclass
from datetime import datetime

from common.validate import gt
from .._dates import parse_ymd


@dataclass(frozen=True, slots=True)
class Window:
    start: str = "2025-01-01"
    days: int = 365

    def __post_init__(self) -> None:
        gt("days", self.days, 0)
        _ = parse_ymd("start", self.start)

    @property
    def start_date(self) -> datetime:
        return parse_ymd("start", self.start)
