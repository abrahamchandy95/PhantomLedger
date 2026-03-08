from datetime import datetime


def parse_ymd(name: str, s: str) -> datetime:
    try:
        return datetime.strptime(s, "%Y-%m-%d")
    except ValueError as e:
        raise ValueError(f"{name} must be YYYY-MM-DD, got {s!r}") from e
