from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

from common.schema import HAS_PAID, RAW_LEDGER
from common.temporal import dt_str
from common.types import Txn
from common.ids import is_external_account
from emit.tg_csv import CsvCell, CsvRow, write_csv


@dataclass(slots=True)
class _Agg:
    total_amount: float
    total_num_txns: int
    first_ts: datetime
    last_ts: datetime


def _aggregate_has_paid(txns: list[Txn]) -> dict[tuple[str, str], _Agg]:
    """
    Aggregate per-transaction ledger into HAS_PAID edge stats.

    Key is (src_acct, dst_acct).
    """
    agg: dict[tuple[str, str], _Agg] = {}

    for t in txns:
        key = (t.src_acct, t.dst_acct)
        rec = agg.get(key)
        if rec is None:
            agg[key] = _Agg(
                total_amount=float(t.amount),
                total_num_txns=1,
                first_ts=t.ts,
                last_ts=t.ts,
            )
            continue

        rec.total_amount += float(t.amount)
        rec.total_num_txns += 1
        if t.ts < rec.first_ts:
            rec.first_ts = t.ts
        if t.ts > rec.last_ts:
            rec.last_ts = t.ts

    return agg


def build_has_paid_rows(txns: list[Txn]) -> list[CsvRow]:
    """
    Build rows for HAS_PAID.csv:
      FROM, TO, total_amount, total_num_txns, first_txn_date, last_txn_date
    """
    agg = _aggregate_has_paid(txns)

    rows: list[CsvRow] = []
    for (src, dst), rec in agg.items():
        row: list[CsvCell] = [
            src,
            dst,
            round(float(rec.total_amount), 2),
            int(rec.total_num_txns),
            dt_str(rec.first_ts),
            dt_str(rec.last_ts),
        ]
        rows.append(row)

    return rows


def build_raw_ledger_rows(txns: list[Txn]) -> list[CsvRow]:
    rows: list[CsvRow] = []
    for t in txns:
        row: list[CsvCell] = [
            t.src_acct,
            t.dst_acct,
            float(t.amount),
            dt_str(t.ts),
            int(t.is_fraud),
            int(t.ring_id),
            t.device_id,
            t.ip_address,
            t.channel,
        ]
        rows.append(row)
    return rows


def emit_transfer_outputs(out_dir: Path, txns: list[Txn], emit_raw_ledger: bool) -> int:
    """
    Writes:
      - HAS_PAID.csv        : internal->internal only (TigerGraph schema-safe)
      - HAS_PAID_full.csv   : all edges including external (generic ledger use)
      - transactions_raw.csv: optional raw ledger of all txns

    Returns: number of unique HAS_PAID edges written (internal-only).
    """
    out_dir.mkdir(parents=True, exist_ok=True)

    internal_txns = [
        t
        for t in txns
        if (not is_external_account(t.src_acct))
        and (not is_external_account(t.dst_acct))
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
