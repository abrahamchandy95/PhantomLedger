from dataclasses import dataclass
from datetime import datetime

from common.timeline import format_datetime
from common.transactions import Transaction
from emit.csv_io import CsvCell, CsvRow


@dataclass(slots=True)
class _Agg:
    total_amount: float
    total_num_txns: int
    first_ts: datetime
    last_ts: datetime


def _aggregate_has_paid(txns: list[Transaction]) -> dict[tuple[str, str], _Agg]:
    """
    Aggregate per-transaction ledger into HAS_PAID edge stats.

    Key is (src_acct, dst_acct).
    """
    agg: dict[tuple[str, str], _Agg] = {}

    for txn in txns:
        key = (txn.source, txn.target)
        rec = agg.get(key)
        if rec is None:
            agg[key] = _Agg(
                total_amount=float(txn.amount),
                total_num_txns=1,
                first_ts=txn.timestamp,
                last_ts=txn.timestamp,
            )
            continue

        rec.total_amount += float(txn.amount)
        rec.total_num_txns += 1
        if txn.timestamp < rec.first_ts:
            rec.first_ts = txn.timestamp
        if txn.timestamp > rec.last_ts:
            rec.last_ts = txn.timestamp

    return agg


def build_has_paid_rows(txns: list[Transaction]) -> list[CsvRow]:
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
            format_datetime(rec.first_ts),
            format_datetime(rec.last_ts),
        ]
        rows.append(row)

    return rows


def build_raw_ledger_rows(txns: list[Transaction]) -> list[CsvRow]:
    rows: list[CsvRow] = []
    for txn in txns:
        row: list[CsvCell] = [
            txn.source,
            txn.target,
            float(txn.amount),
            format_datetime(txn.timestamp),
            int(txn.fraud_flag),
            int(txn.fraud_ring_id),
            txn.device_id,
            txn.ip_address,
            txn.channel,
        ]
        rows.append(row)
    return rows
