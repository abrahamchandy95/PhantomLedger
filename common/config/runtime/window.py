from dataclasses import dataclass
from datetime import datetime

from common.validate import require_int_gt

from .._dates import parse_ymd


@dataclass(frozen=True, slots=True)
class WindowConfig:
    start: str = "2025-01-01"
    days: int = 180

    def start_date(self) -> datetime:
        return parse_ymd("start", self.start)

    def validate(self) -> None:
        require_int_gt("days", self.days, 0)
        _ = self.start_date()
