from datetime import datetime

from common.types import Txn

type TxnSortKey = tuple[datetime, str, str, float, int, int, str, str, str]


def txn_sort_key(txn: Txn) -> TxnSortKey:
    return (
        txn.ts,
        txn.src_acct,
        txn.dst_acct,
        float(txn.amount),
        int(txn.is_fraud),
        int(txn.ring_id),
        txn.channel or "",
        txn.device_id or "",
        txn.ip_address or "",
    )
