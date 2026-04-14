from pathlib import Path

from common.progress import maybe_tqdm, status
from common.run import UseCase
from common.schema import ML_PARTY, ML_TRANSFER, ML_ACCOUNT_DEVICE, ML_ACCOUNT_IP
from pipeline.result import SimulationResult

from export.csv_io import write as write_csv
from export.registry import register
from export.standard import export as export_standard

from .party import rows as party_rows
from .transfer import rows as transfer_rows
from .infra_edges import device_rows, ip_rows


@register(UseCase.MULE_ML)
def export(
    result: SimulationResult,
    out_dir: Path,
    show_transactions: bool,
    include_standard_export: bool = True,
) -> None:
    # Optional base graph tables (vertices, edges, has_paid, optional ledger)
    if include_standard_export:
        export_standard(result, out_dir, show_transactions, include_standard_export)

    # ML-specific tables
    ml_dir = out_dir / "ml_ready"
    ml_dir.mkdir(parents=True, exist_ok=True)

    e, i, t = result.entities, result.infra, result.transfers
    status("Export: writing mule-ml tables...")

    ml_tables = [
        (ML_PARTY, party_rows(e, i, t)),
        (ML_TRANSFER, transfer_rows(t)),
        (ML_ACCOUNT_DEVICE, device_rows(e, i, t)),
        (ML_ACCOUNT_IP, ip_rows(e, i, t)),
    ]

    for spec, rows in maybe_tqdm(
        ml_tables,
        desc="mule-ml export",
        unit="table",
        leave=False,
    ):
        write_csv(ml_dir / spec.filename, spec.header, rows)
