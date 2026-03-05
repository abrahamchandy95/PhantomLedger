import csv
from collections.abc import Iterable, Sequence
from datetime import datetime
from pathlib import Path

CsvCell = str | int | float | bool | None
CsvRow = Sequence[CsvCell]


def dt_str(dt: datetime) -> str:
    """TigerGraph loader-friendly datetime string."""
    return dt.strftime("%Y-%m-%d %H:%M:%S")


def ensure_dir(path: str | Path) -> None:
    Path(path).mkdir(parents=True, exist_ok=True)


def write_csv(path: str | Path, header: CsvRow, rows: Iterable[CsvRow]) -> None:
    """
    Write a CSV with a header row.

    header: one row of CSV cells (typically strings)
    rows: iterable of row sequences
    """
    p = Path(path)
    if p.parent != Path("."):
        ensure_dir(p.parent)

    with p.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(tuple(header))
        w.writerows(rows)
