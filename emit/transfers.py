from dataclasses import dataclass
from datetime import datetime

from common.temporal import dt_str
from common.types import Txn
from emit.csv_io import CsvCell, CsvRow


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

    for txn in txns:
        key = (txn.src_acct, txn.dst_acct)
        rec = agg.get(key)
        if rec is None:
            agg[key] = _Agg(
                total_amount=float(txn.amount),
                total_num_txns=1,
                first_ts=txn.ts,
                last_ts=txn.ts,
            )
            continue

        rec.total_amount += float(txn.amount)
        rec.total_num_txns += 1
        if txn.ts < rec.first_ts:
            rec.first_ts = txn.ts
        if txn.ts > rec.last_ts:
            rec.last_ts = txn.ts

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
    for txn in txns:
        row: list[CsvCell] = [
            txn.src_acct,
            txn.dst_acct,
            float(txn.amount),
            dt_str(txn.ts),
            int(txn.is_fraud),
            int(txn.ring_id),
            txn.device_id,
            txn.ip_address,
            txn.channel,
        ]
        rows.append(row)
    return rows
