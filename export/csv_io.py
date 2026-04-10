import csv
from collections.abc import Iterable, Sequence
from pathlib import Path

type Cell = str | int | float | bool | None
type Row = Sequence[Cell]


def write(path: str | Path, header: Row, rows: Iterable[Row]) -> None:
    """
    Write an iterable of rows to a CSV file with a header.
    """
    p = Path(path)

    p.parent.mkdir(parents=True, exist_ok=True)

    with p.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)
