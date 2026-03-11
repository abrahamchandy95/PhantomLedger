from pathlib import Path

from common.ids import is_external_account
from common.schema import HAS_PAID, RAW_LEDGER
from common.transactions import Transaction
from emit.csv_io import write_csv
from emit.transfers import build_has_paid_rows, build_raw_ledger_rows


def emit_transfer_outputs(
    out_dir: Path, txns: list[Transaction], emit_raw_ledger: bool
) -> int:
    """
    Writes:
      - HAS_PAID.csv        : internal->internal only (TigerGraph schema-safe)
      - HAS_PAID_full.csv   : all edges including external (generic ledger use)
      - transactions_raw.csv: optional raw ledger of all txns

    Returns: number of unique HAS_PAID edges written (internal-only).
    """
    out_dir.mkdir(parents=True, exist_ok=True)

    internal_txns = [
        txn
        for txn in txns
        if (not is_external_account(txn.source))
        and (not is_external_account(txn.target))
    ]

    # TG-safe
    has_paid_rows = build_has_paid_rows(internal_txns)
    write_csv(out_dir / HAS_PAID.filename, HAS_PAID.header, has_paid_rows)

    # Full (generic ledger)
    has_paid_full_rows = build_has_paid_rows(txns)
    write_csv(out_dir / "HAS_PAID_full.csv", HAS_PAID.header, has_paid_full_rows)

    if emit_raw_ledger:
        ledger_rows = build_raw_ledger_rows(txns)
        write_csv(out_dir / RAW_LEDGER.filename, RAW_LEDGER.header, ledger_rows)

    return len(has_paid_rows)
