"""
Vertex row generators for TigerGraph AML_Schema_V1.

Each function yields tuples matching the header defined in schema.py.
The main exporter writes them to CSV.
"""

from __future__ import annotations

from collections.abc import Iterator
from datetime import datetime, timedelta

from common.ids import is_external
from common.timeline import format_ts
from common.transactions import Transaction
from export.csv_io import Cell
from pipeline.state import Entities, Infra, Transfers

import export.aml.identity_aml as identity
from export.aml.sar import SarRecord
from export.aml.shared import all_bank_ids, counterparty_bank_id

type Row = tuple[Cell, ...]


# ── Shared helpers ───────────────────────────────────────────────


def _last_transaction_by_account(transfers: Transfers) -> dict[str, datetime]:
    out: dict[str, datetime] = {}
    for txn in transfers.final_txns:
        for acct_id in (txn.source, txn.target):
            prev = out.get(acct_id)
            if prev is None or txn.timestamp > prev:
                out[acct_id] = txn.timestamp
    return out


def _final_balance_by_account(transfers: Transfers) -> dict[str, float]:
    final_book = transfers.final_book
    if final_book is None:
        raise ValueError(
            "Transfers.final_book is missing. "
            + "Run the post-fraud replay in pipeline/stages/transfers.py first."
        )

    out: dict[str, float] = {}
    for acct_id, idx in final_book.account_indices.items():
        balance = final_book.balances.item(idx)
        out[acct_id] = round(balance, 2)

    return out


def _account_open_date(
    acct_id: str,
    entities: Entities,
    sim_start: datetime,
) -> datetime:
    owner = entities.accounts.owner_map.get(acct_id)
    if owner is None:
        return sim_start - timedelta(days=365)
    return identity.onboarding_date(owner, sim_start)


# ── Customer ─────────────────────────────────────────────────────


def customer_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    people = entities.people

    for person_id in people.ids:
        persona = entities.persona_map.get(person_id, "salaried")
        is_fraud = person_id in people.frauds
        is_mule = person_id in people.mules
        is_victim = person_id in people.victims

        yield (
            person_id,
            identity.customer_type(persona),
            "active",
            identity.marital_status(person_id, persona),
            identity.networth_code(persona),
            identity.income_code(persona),
            identity.occupation(person_id, persona),
            identity.risk_rating(
                is_fraud=is_fraud,
                is_mule=is_mule,
                is_victim=is_victim,
            ),
            "US",
            format_ts(identity.onboarding_date(person_id, sim_start)),
        )


def _account_type(acct_id: str, credit_card_ids: set[str]) -> str:
    if acct_id in credit_card_ids:
        return "credit"
    if acct_id.startswith("BOP"):
        return "business_checking"
    if acct_id.startswith("BRK"):
        return "brokerage"
    if acct_id.startswith("L"):
        return "credit"
    return "checking"


# ── Account ──────────────────────────────────────────────────────


def account_rows(
    entities: Entities,
    transfers: Transfers,
    sim_start: datetime,
) -> Iterator[Row]:
    """Emit internal (non-external) accounts as Account vertices."""
    accounts = entities.accounts
    last_txn = _last_transaction_by_account(transfers)
    balance_by_account = _final_balance_by_account(transfers)

    credit_card_ids: set[str] = (
        set(entities.credit_cards.ids) if entities.credit_cards else set()
    )

    for acct_id in accounts.ids:
        if is_external(acct_id):
            continue
        if acct_id in accounts.externals:
            continue

        open_date = _account_open_date(acct_id, entities, sim_start)
        acct_type = _account_type(acct_id, credit_card_ids)
        branch_code = f"BR{(hash(acct_id) % 50) + 1:03d}"
        last_dt = last_txn.get(acct_id)

        yield (
            acct_id,
            acct_id,
            balance_by_account.get(acct_id, 0.0),
            format_ts(open_date),
            "active",
            format_ts(last_dt) if last_dt else "",
            "",
            acct_type,
            "USD",
            branch_code,
        )


# ── Counterparty ─────────────────────────────────────────────────


def counterparty_rows(entities: Entities) -> Iterator[Row]:
    for cp_id in sorted(entities.accounts.externals):
        bank_id = counterparty_bank_id(cp_id)
        bank_nm = identity.name_for_bank(bank_id)
        nm = identity.name_for_counterparty(cp_id)

        yield (
            cp_id,
            nm.first_name,
            identity.routing_number_for_id(bank_id),
            bank_nm.first_name,
            "US",
        )


# ── Name ─────────────────────────────────────────────────────────


def name_rows(entities: Entities) -> Iterator[Row]:
    """Emit Name vertices for customers, counterparties, and banks."""
    emitted: set[str] = set()

    for person_id in entities.people.ids:
        nm = identity.name_for_person(person_id)
        if nm.id not in emitted:
            emitted.add(nm.id)
            yield (nm.id, nm.first_name, nm.middle_name, nm.last_name)

    for cp_id in sorted(entities.accounts.externals):
        nm = identity.name_for_counterparty(cp_id)
        if nm.id not in emitted:
            emitted.add(nm.id)
            yield (nm.id, nm.first_name, nm.middle_name, nm.last_name)

    for bank_id in sorted(all_bank_ids(entities.accounts.externals)):
        nm = identity.name_for_bank(bank_id)
        if nm.id not in emitted:
            emitted.add(nm.id)
            yield (nm.id, nm.first_name, nm.middle_name, nm.last_name)


# ── Address ──────────────────────────────────────────────────────


def _emit_address(addr: identity.AddressRecord) -> Row:
    return (
        addr.id,
        addr.street_line1,
        addr.street_line2,
        addr.city,
        addr.state,
        addr.postal_code,
        addr.country,
        addr.address_type,
        int(addr.is_high_risk_geo),
    )


def address_rows(entities: Entities) -> Iterator[Row]:
    """Emit Address vertices for customers, counterparties, and banks."""
    emitted: set[str] = set()

    for person_id in entities.people.ids:
        addr = identity.address_for_person(person_id)
        if addr.id not in emitted:
            emitted.add(addr.id)
            yield _emit_address(addr)

    for cp_id in sorted(entities.accounts.externals):
        addr = identity.address_for_counterparty(cp_id)
        if addr.id not in emitted:
            emitted.add(addr.id)
            yield _emit_address(addr)

    for bank_id in sorted(all_bank_ids(entities.accounts.externals)):
        addr = identity.address_for_bank(bank_id)
        if addr.id not in emitted:
            emitted.add(addr.id)
            yield _emit_address(addr)


# ── Country ──────────────────────────────────────────────────────


def country_rows() -> Iterator[Row]:
    yield ("US", "United States", 0.1)


# ── Device ───────────────────────────────────────────────────────


def device_rows(infra: Infra) -> Iterator[Row]:
    devices = infra.devices

    device_ips: dict[str, str] = {}
    for rec in devices.has_used:
        person_ips = infra.ips.by_person.get(rec.person_id, [])
        if person_ips and rec.device_id not in device_ips:
            device_ips[rec.device_id] = person_ips[0]

    first_seen: dict[str, datetime] = {}
    last_seen: dict[str, datetime] = {}
    for rec in devices.has_used:
        did = rec.device_id
        if did not in first_seen or rec.first_seen < first_seen[did]:
            first_seen[did] = rec.first_seen
        if did not in last_seen or rec.last_seen > last_seen[did]:
            last_seen[did] = rec.last_seen

    os_map = {
        "android": "Android",
        "ios": "iOS",
        "web": "Browser",
        "desktop": "Windows",
    }

    for device_id, (device_type, flagged) in devices.registry.items():
        ip = device_ips.get(device_id, "0.0.0.0")
        is_vpn = bool(flagged)
        os_type = os_map.get(device_type, "Unknown")

        yield (
            device_id,
            device_type,
            ip,
            "US",
            os_type,
            format_ts(first_seen.get(device_id, datetime(2025, 1, 1))),
            format_ts(last_seen.get(device_id, datetime(2025, 12, 31))),
            int(is_vpn),
        )


# ── Transaction ──────────────────────────────────────────────────

_CHANNEL_PURPOSE: dict[str, str] = {
    "salary": "payroll",
    "rent": "rent_payment",
    "rent_ach": "rent_payment",
    "rent_portal": "rent_payment",
    "rent_p2p": "rent_payment",
    "rent_check": "rent_payment",
    "p2p": "peer_transfer",
    "bill": "bill_payment",
    "merchant": "purchase",
    "card_purchase": "purchase",
    "subscription": "subscription",
    "atm_withdrawal": "cash_withdrawal",
    "self_transfer": "internal_transfer",
    "external_unknown": "wire_transfer",
    "insurance_premium": "insurance",
    "insurance_claim": "insurance_claim",
    "mortgage_payment": "loan_payment",
    "auto_loan_payment": "loan_payment",
    "student_loan_payment": "loan_payment",
    "tax_estimated_payment": "tax_payment",
    "tax_balance_due": "tax_payment",
    "tax_refund": "tax_refund",
    "gov_social_security": "government_benefit",
    "gov_pension": "government_benefit",
    "gov_disability": "government_benefit",
    "cc_payment": "credit_card_payment",
    "cc_interest": "interest_charge",
    "cc_late_fee": "fee",
    "cc_refund": "refund",
    "cc_chargeback": "chargeback",
    "client_ach_credit": "business_income",
    "card_settlement": "business_income",
    "platform_payout": "business_income",
    "owner_draw": "business_draw",
    "investment_inflow": "investment",
}

_CREDIT_CHANNELS: frozenset[str] = frozenset(
    {
        "salary",
        "gov_social_security",
        "gov_pension",
        "gov_disability",
        "tax_refund",
        "insurance_claim",
        "client_ach_credit",
        "card_settlement",
        "platform_payout",
        "owner_draw",
        "investment_inflow",
        "cc_refund",
        "cc_chargeback",
    }
)


def _risk_score(txn: Transaction) -> int:
    if txn.fraud_flag == 1:
        return 10
    ch = txn.channel or ""
    if ch.startswith("camouflage"):
        return 3
    return 0


def _credit_debit(txn: Transaction) -> str:
    ch = txn.channel or ""
    if ch in _CREDIT_CHANNELS:
        return "C"
    return "D"


def transaction_rows(transfers: Transfers) -> Iterator[Row]:
    for idx, txn in enumerate(transfers.final_txns, start=1):
        txn_id = f"TXN_{idx:012d}"
        channel = txn.channel or "unknown"
        purpose = _CHANNEL_PURPOSE.get(channel, "other")

        yield (
            txn_id,
            _credit_debit(txn),
            format_ts(txn.timestamp),
            channel,
            purpose,
            _risk_score(txn),
            "USD",
            round(float(txn.amount), 2),
            1.0,
            round(float(txn.amount), 2),
        )


# ── SAR ──────────────────────────────────────────────────────────


def sar_rows(sars: list[SarRecord]) -> Iterator[Row]:
    for sar in sars:
        yield (
            sar.sar_id,
            format_ts(sar.filing_date),
            sar.amount_involved,
            format_ts(sar.activity_start),
            format_ts(sar.activity_end),
            sar.violation_type,
        )


# ── Bank ─────────────────────────────────────────────────────────


def bank_rows(entities: Entities) -> Iterator[Row]:
    for bank_id in sorted(all_bank_ids(entities.accounts.externals)):
        yield (bank_id,)


# ── Watchlist ────────────────────────────────────────────────────


def watchlist_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    people = entities.people
    for person_id in sorted(people.frauds | people.mules):
        yield (
            f"WL_{person_id}",
            "internal_fraud_list",
            "fraud_suspect",
            format_ts(sim_start),
        )
