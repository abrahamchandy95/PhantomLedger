from pathlib import Path

from common.run import UseCase
from common.schema import HAS_PAID, LEDGER
from pipeline.result import SimulationResult

from export.csv_io import write as write_csv
from export.registry import register

from .tables import build as build_tables
from .transfers import has_paid, ledger


@register(UseCase.STANDARD)
def export(
    result: SimulationResult,
    out_dir: Path,
    show_transactions: bool,
) -> None:
    has_paid_rows = list(has_paid(result.transfers.final_txns))
    write_csv(out_dir / HAS_PAID.filename, HAS_PAID.header, has_paid_rows)

    if show_transactions:
        write_csv(
            out_dir / LEDGER.filename,
            LEDGER.header,
            ledger(result.transfers.final_txns),
        )

    for spec, rows in build_tables(result.entities, result.infra):
        write_csv(out_dir / spec.filename, spec.header, rows)
