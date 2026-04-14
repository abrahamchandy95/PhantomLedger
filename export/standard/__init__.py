from pathlib import Path

from common.progress import maybe_tqdm, status
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
    include_standard_export: bool = True,
) -> None:
    _ = include_standard_export

    status("Export: writing standard CSV tables...")
    write_csv(
        out_dir / HAS_PAID.filename,
        HAS_PAID.header,
        has_paid(result.transfers.final_txns),
    )

    if show_transactions:
        write_csv(
            out_dir / LEDGER.filename,
            LEDGER.header,
            ledger(result.transfers.final_txns),
        )

    for spec, rows in maybe_tqdm(
        list(build_tables(result.entities, result.infra)),
        desc="standard export",
        unit="table",
        leave=False,
    ):
        write_csv(out_dir / spec.filename, spec.header, rows)
