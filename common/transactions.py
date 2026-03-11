from dataclasses import dataclass
from datetime import datetime


@dataclass(frozen=True, slots=True)
class Transaction:
    """
    Single transfer event in the raw ledger.

    fraud_flag:
      0 = legitimate
      1 = illicit / fraud-linked

    fraud_ring_id:
      -1 = none
      >=0 = fraud ring identifier

    device_id / ip_address:
      Optional: infra attribution for the initiating side (usually source).

    channel:
      Optional: "salary", "rent", "p2p", "merchant", "external_unknown",
                "fraud_layering", "fraud_structuring", "camouflage", etc.
    """

    source: str
    target: str
    amount: float
    timestamp: datetime

    fraud_flag: int
    fraud_ring_id: int

    device_id: str | None = None
    ip_address: str | None = None
    channel: str | None = None
