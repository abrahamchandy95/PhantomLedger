from pathlib import Path

from common.run import UseCase
from common.schema import ML_PARTY, ML_TRANSFER, ML_ACCOUNT_DEVICE, ML_ACCOUNT_IP
from pipeline.result import SimulationResult

from export.csv_io import write as write_csv
from export.registry import register
from export.standard import export as export_standard

from .party import rows as party_rows
from .transfer import rows as transfer_rows
from .edges import device_rows, ip_rows


@register(UseCase.MULE_ML)
def export(
    result: SimulationResult,
    out_dir: Path,
    show_transactions: bool,
) -> None:
    # Base graph tables (vertices, edges, has_paid, optional ledger)
    export_standard(result, out_dir, show_transactions)

    # ML-specific tables
    ml_dir = out_dir / "ml_ready"
    ml_dir.mkdir(parents=True, exist_ok=True)

    e, i, t = result.entities, result.infra, result.transfers

    write_csv(ml_dir / ML_PARTY.filename, ML_PARTY.header, party_rows(e, i, t))
    write_csv(ml_dir / ML_TRANSFER.filename, ML_TRANSFER.header, transfer_rows(t))
    write_csv(
        ml_dir / ML_ACCOUNT_DEVICE.filename,
        ML_ACCOUNT_DEVICE.header,
        device_rows(e, i, t),
    )
    write_csv(ml_dir / ML_ACCOUNT_IP.filename, ML_ACCOUNT_IP.header, ip_rows(e, i, t))
