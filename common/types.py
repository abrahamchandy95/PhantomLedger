from dataclasses import dataclass
from datetime import datetime


@dataclass(frozen=True, slots=True)
class Txn:
    """
    Single transfer event in the raw ledger.

    is_fraud:
      0 = legitimate
      1 = illicit / fraud-linked

    ring_id:
      -1 = none
      >=0 = fraud ring identifier

    device_id / ip_address:
      Optional: infra attribution for the initiating side (usually src).
    channel:
      Optional: "salary", "rent", "p2p", "merchant", "external_unknown",
                "fraud_layering", "fraud_structuring", "camouflage", etc.
    """

    src_acct: str
    dst_acct: str
    amount: float
    ts: datetime
    is_fraud: int
    ring_id: int

    device_id: str | None = None
    ip_address: str | None = None
    channel: str | None = None
