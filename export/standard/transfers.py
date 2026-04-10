from collections.abc import Iterable, Iterator
from dataclasses import dataclass
from datetime import datetime

from common.timeline import format_ts
from common.transactions import Transaction
from ..csv_io import Row


@dataclass(slots=True)
class _Agg:
    total_amount: float
    total_num_txns: int
    first_ts: datetime
    last_ts: datetime


def _aggregate_has_paid(txns: Iterable[Transaction]) -> dict[tuple[str, str], _Agg]:
    """
    Reduces the transaction log into aggregate edge metrics.
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
        else:
            rec.total_amount += float(txn.amount)
            rec.total_num_txns += 1

            if txn.timestamp < rec.first_ts:
                rec.first_ts = txn.timestamp
            elif txn.timestamp > rec.last_ts:
                rec.last_ts = txn.timestamp

    return agg


# --- Target Schema Generators ---


def has_paid(txns: Iterable[Transaction]) -> Iterator[Row]:
    """
    Yields rows for the HAS_PAID edge table.
    (src, dst, total_amount, total_num_txns, first_ts, last_ts)
    """
    agg = _aggregate_has_paid(txns)

    for (src, dst), rec in sorted(agg.items()):
        yield (
            src,
            dst,
            round(rec.total_amount, 2),
            rec.total_num_txns,
            format_ts(rec.first_ts),
            format_ts(rec.last_ts),
        )


def ledger(txns: Iterable[Transaction]) -> Iterator[Row]:
    """
    Yields rows for the raw transaction ledger.
    Streams directly to disk to prevent OOM errors on massive datasets.
    """
    for txn in txns:
        yield (
            txn.source,
            txn.target,
            float(txn.amount),
            format_ts(txn.timestamp),
            int(txn.fraud_flag),
            int(txn.fraud_ring_id),
            txn.device_id,
            txn.ip_address,
            txn.channel,
        )
