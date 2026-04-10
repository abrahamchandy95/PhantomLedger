from collections.abc import Iterator

from common.schema import ML_TRANSFER
from common.timeline import format_ts
from pipeline.state import Transfers
from ..csv_io import Row


HEADER = ML_TRANSFER.header


def rows(transfers: Transfers) -> Iterator[Row]:
    for idx, txn in enumerate(transfers.final_txns, start=1):
        yield (
            f"T{idx:012d}",
            txn.source,
            txn.target,
            round(float(txn.amount), 2),
            format_ts(txn.timestamp),
        )
