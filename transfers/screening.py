from collections.abc import Sequence
from datetime import datetime

from common.transactions import Transaction
from transfers.balances import Ledger


def advance_book_through(
    book: Ledger | None,
    base_txns: Sequence[Transaction],
    start_idx: int,
    until_ts: datetime,
    *,
    inclusive: bool,
) -> int:
    if book is None:
        return start_idx

    idx = start_idx
    while idx < len(base_txns):
        txn = base_txns[idx]

        if inclusive:
            if txn.timestamp > until_ts:
                break
        else:
            if txn.timestamp >= until_ts:
                break

        _ = book.try_transfer_with_reason(
            txn.source,
            txn.target,
            txn.amount,
            channel=txn.channel,
            timestamp=txn.timestamp,
        )
        idx += 1

    return idx
