"""
SAR (Suspicious Activity Report) generation.

FinCEN requires financial institutions to file a SAR when they detect
or suspect transactions involving potential money laundering, structuring,
terrorist financing, or other illegal activity exceeding $5,000
(31 CFR § 1020.320).

This module synthesizes SARs from the simulation's ground-truth fraud
data. Each fraud ring and each solo fraudster produces one SAR.
The SAR's temporal window and amount are derived from the actual
illicit transactions in the simulation output.

Violation type mapping (from fraud typology to BSA/AML violation):
  classic      → money_laundering
  layering     → money_laundering
  funnel       → money_laundering
  structuring  → structuring
  invoice      → suspicious_activity
  mule         → money_laundering
  solo         → suspicious_activity
"""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from datetime import datetime, timedelta

from common.transactions import Transaction
from entities import models


@dataclass(frozen=True, slots=True)
class SarRecord:
    sar_id: str
    filing_date: datetime
    amount_involved: float
    activity_start: datetime
    activity_end: datetime
    violation_type: str
    subject_person_ids: list[str]
    subject_roles: list[str]
    covered_account_ids: list[str]
    covered_amounts: list[float]


def _violation_type_for_ring(txns: list[Transaction]) -> str:
    """Infer violation type from the dominant fraud channel in the ring."""
    channel_counts: dict[str, int] = defaultdict(int)
    for txn in txns:
        if txn.channel:
            channel_counts[txn.channel] += 1

    if not channel_counts:
        return "suspicious_activity"

    dominant = max(channel_counts, key=lambda ch: channel_counts[ch])

    if "structuring" in dominant:
        return "structuring"
    if "invoice" in dominant:
        return "suspicious_activity"
    return "money_laundering"


def _ring_txns(
    ring_id: int,
    all_txns: list[Transaction],
) -> list[Transaction]:
    return [t for t in all_txns if t.fraud_ring_id == ring_id and t.fraud_flag == 1]


def _account_activity_amounts(
    txns: list[Transaction],
    internal_accounts: set[str],
) -> dict[str, float]:
    """Sum suspicious money that flowed through each internal account.

    Both source and target sides are attributed: a $1,000 transfer
    from A to B means $1,000 of suspicious activity touched A (outflow)
    and $1,000 touched B (inflow). This ensures mule accounts that
    only received illicit funds still appear in sar_covers.

    The sum of activity_amount across covered accounts will therefore
    exceed SAR.amount_involved. That is intentional: activity_amount
    is per-account throughput, not a share of the total.

    Only accounts owned by the bank (internal_accounts) are included;
    external counterparties are outside the bank's SAR scope.
    """
    amounts: dict[str, float] = defaultdict(float)
    for txn in txns:
        if txn.source in internal_accounts:
            amounts[txn.source] += txn.amount
        if txn.target in internal_accounts:
            amounts[txn.target] += txn.amount
    return dict(amounts)


def generate_sars(
    people: models.People,
    accounts: models.Accounts,
    final_txns: list[Transaction],
) -> list[SarRecord]:
    """
    Generate one SAR per fraud ring + one per solo fraudster.

    Filing date is 30 days after the last illicit activity in the ring,
    matching the BSA filing deadline of 30 calendar days from initial
    detection.
    """
    sars: list[SarRecord] = []
    internal_accounts = set(accounts.owner_map.keys())

    # ── Ring SARs ────────────────────────────────────────────────
    for ring in people.rings:
        txns = _ring_txns(ring.id, final_txns)
        if not txns:
            continue

        total_amount = sum(t.amount for t in txns)
        activity_start = min(t.timestamp for t in txns)
        activity_end = max(t.timestamp for t in txns)
        filing_date = activity_end + timedelta(days=30)

        violation = _violation_type_for_ring(txns)

        subjects: list[str] = []
        roles: list[str] = []
        for pid in ring.frauds:
            subjects.append(pid)
            roles.append("subject")
        for pid in ring.mules:
            subjects.append(pid)
            roles.append("beneficiary")

        acct_amounts = _account_activity_amounts(txns, internal_accounts)
        covered_ids = sorted(acct_amounts.keys())
        covered_amts = [round(acct_amounts[aid], 2) for aid in covered_ids]

        sars.append(
            SarRecord(
                sar_id=f"SAR_RING_{ring.id:04d}",
                filing_date=filing_date,
                amount_involved=round(total_amount, 2),
                activity_start=activity_start,
                activity_end=activity_end,
                violation_type=violation,
                subject_person_ids=subjects,
                subject_roles=roles,
                covered_account_ids=covered_ids,
                covered_amounts=covered_amts,
            )
        )

    # ── Solo fraudster SARs ──────────────────────────────────────
    for person_id in sorted(people.solo_frauds):
        person_accts = set(accounts.by_person.get(person_id, []))
        if not person_accts:
            continue

        solo_txns = [
            t
            for t in final_txns
            if t.fraud_flag == 1
            and t.fraud_ring_id == -1
            and (t.source in person_accts or t.target in person_accts)
        ]
        if not solo_txns:
            continue

        total_amount = sum(t.amount for t in solo_txns)
        activity_start = min(t.timestamp for t in solo_txns)
        activity_end = max(t.timestamp for t in solo_txns)
        filing_date = activity_end + timedelta(days=30)

        acct_amounts = _account_activity_amounts(solo_txns, person_accts)
        covered_ids = sorted(acct_amounts.keys())
        covered_amts = [round(acct_amounts[aid], 2) for aid in covered_ids]

        sars.append(
            SarRecord(
                sar_id=f"SAR_SOLO_{person_id}",
                filing_date=filing_date,
                amount_involved=round(total_amount, 2),
                activity_start=activity_start,
                activity_end=activity_end,
                violation_type="suspicious_activity",
                subject_person_ids=[person_id],
                subject_roles=["subject"],
                covered_account_ids=covered_ids,
                covered_amounts=covered_amts,
            )
        )

    return sars
