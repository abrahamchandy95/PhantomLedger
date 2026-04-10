from datetime import datetime

from common.transactions import Transaction

type Key = tuple[datetime, str, str, float, int, int, str, str, str]


def key(txn: Transaction) -> Key:
    """
    Deterministic sorting key for transactions.
    """
    return (
        txn.timestamp,
        txn.source,
        txn.target,
        float(txn.amount),
        int(txn.fraud_flag),
        int(txn.fraud_ring_id),
        txn.channel or "",
        txn.device_id or "",
        txn.ip_address or "",
    )
