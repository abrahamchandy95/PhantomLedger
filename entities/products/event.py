"""
Obligation events emitted by financial products.

Every product (mortgage, auto loan, insurance policy, etc.) exposes
a scheduled_events() method that yields ObligationEvents. These are
pure data — no infrastructure routing, no fraud flags, no device/IP.
The transfer emitters consume them, resolve accounts, attach infra,
and produce final Transactions.

This separation means adding a new product type never touches the
transaction pipeline — it just yields more ObligationEvents.
"""

from dataclasses import dataclass
from datetime import datetime
from enum import StrEnum


class Direction(StrEnum):
    """Which way money flows relative to the account holder."""

    OUTFLOW = "outflow"
    INFLOW = "inflow"


@dataclass(frozen=True, slots=True)
class ObligationEvent:
    """A single scheduled or stochastic financial event from a product."""

    # Who owes / receives
    person_id: str
    direction: Direction

    # External counterparty account (lender, insurer, IRS, etc.)
    counterparty_acct: str

    # Payment details
    amount: float
    timestamp: datetime
    channel: str

    product_type: str = ""
    product_id: str = ""
