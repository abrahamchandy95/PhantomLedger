"""
Edge row generators for TigerGraph AML_Schema_V1.

Each function yields tuples matching the header defined in schema.py.
Edge rows are (FROM_id, TO_id, ...attributes).
"""

from __future__ import annotations

from collections.abc import Iterator
from datetime import datetime

from common.ids import is_external
from common.timeline import format_ts
from export.csv_io import Cell
from pipeline.state import Entities, Infra, Transfers

import export.aml.identity_aml as identity
import export.aml.minhash as minhash
from export.aml.sar import SarRecord
from export.aml.shared import all_bank_ids, counterparty_bank_id

type Row = tuple[Cell, ...]


# ── customer_has_account / account_has_primary_customer ───────────


def customer_has_account_rows(entities: Entities) -> Iterator[Row]:
    for person_id, acct_ids in entities.accounts.by_person.items():
        for acct_id in acct_ids:
            if is_external(acct_id) or acct_id in entities.accounts.externals:
                continue
            yield (person_id, acct_id)


def account_has_primary_customer_rows(entities: Entities) -> Iterator[Row]:
    """One edge per internal account to its owner."""
    for person_id, acct_ids in entities.accounts.by_person.items():
        for acct_id in acct_ids:
            if is_external(acct_id) or acct_id in entities.accounts.externals:
                continue
            yield (acct_id, person_id)


# ── Transaction edges ────────────────────────────────────────────


def _is_ext(acct_id: str, externals: set[str]) -> bool:
    return is_external(acct_id) or acct_id in externals


def transaction_edge_rows(
    entities: Entities,
    transfers: Transfers,
) -> tuple[
    list[Row],  # send_transaction
    list[Row],  # receive_transaction
    list[Row],  # counterparty_send_transaction
    list[Row],  # counterparty_receive_transaction
    set[tuple[str, str]],  # (internal_acct, external_cp) sent-to pairs
    set[tuple[str, str]],  # (internal_acct, external_cp) received-from pairs
    set[str],  # counterparties observed as senders
    set[str],  # counterparties observed as receivers
]:
    externals = entities.accounts.externals
    cp_names: dict[str, str] = {}

    send: list[Row] = []
    receive: list[Row] = []
    cp_send: list[Row] = []
    cp_receive: list[Row] = []
    sent_to_cp: set[tuple[str, str]] = set()
    received_from_cp: set[tuple[str, str]] = set()
    cp_senders: set[str] = set()
    cp_receivers: set[str] = set()

    for idx, txn in enumerate(transfers.final_txns, start=1):
        txn_id = f"TXN_{idx:012d}"
        src = txn.source
        dst = txn.target
        src_ext = _is_ext(src, externals)
        dst_ext = _is_ext(dst, externals)

        if src_ext:
            name = cp_names.get(src)
            if name is None:
                name = identity.name_for_counterparty(src).first_name
                cp_names[src] = name
            cp_send.append((src, txn_id, name))
            cp_senders.add(src)
            if not dst_ext:
                received_from_cp.add((dst, src))
        else:
            send.append((src, txn_id))
            if dst_ext:
                sent_to_cp.add((src, dst))

        if dst_ext:
            name = cp_names.get(dst)
            if name is None:
                name = identity.name_for_counterparty(dst).first_name
                cp_names[dst] = name
            cp_receive.append((dst, txn_id, name))
            cp_receivers.add(dst)
        else:
            receive.append((dst, txn_id))

    return (
        send,
        receive,
        cp_send,
        cp_receive,
        sent_to_cp,
        received_from_cp,
        cp_senders,
        cp_receivers,
    )


def sent_transaction_to_counterparty_rows(
    pairs: set[tuple[str, str]],
) -> Iterator[Row]:
    for acct, cp in sorted(pairs):
        yield (acct, cp)


def received_transaction_from_counterparty_rows(
    pairs: set[tuple[str, str]],
) -> Iterator[Row]:
    for acct, cp in sorted(pairs):
        yield (acct, cp)


# ── Device edges ─────────────────────────────────────────────────


def uses_device_rows(infra: Infra) -> Iterator[Row]:
    """Customer → Device edges with session metadata."""
    agg: dict[tuple[str, str], tuple[datetime, datetime, int]] = {}

    for rec in infra.devices.has_used:
        key = (rec.person_id, rec.device_id)
        if key not in agg:
            agg[key] = (rec.first_seen, rec.last_seen, 1)
        else:
            fs, ls, cnt = agg[key]
            agg[key] = (
                min(fs, rec.first_seen),
                max(ls, rec.last_seen),
                cnt + 1,
            )

    for (person_id, device_id), (fs, ls, cnt) in sorted(agg.items()):
        yield (person_id, device_id, format_ts(fs), format_ts(ls), cnt)


def logged_from_rows(entities: Entities, infra: Infra) -> Iterator[Row]:
    """Account → Device edges for all internal accounts owned by each person."""
    agg: dict[tuple[str, str], tuple[datetime, datetime, int]] = {}
    externals = entities.accounts.externals

    for rec in infra.devices.has_used:
        person_id = rec.person_id
        accts = entities.accounts.by_person.get(person_id, [])

        for acct_id in accts:
            if is_external(acct_id) or acct_id in externals:
                continue

            key = (acct_id, rec.device_id)
            if key not in agg:
                agg[key] = (rec.first_seen, rec.last_seen, 1)
            else:
                fs, ls, cnt = agg[key]
                agg[key] = (
                    min(fs, rec.first_seen),
                    max(ls, rec.last_seen),
                    cnt + 1,
                )

    for (acct_id, device_id), (fs, ls, cnt) in sorted(agg.items()):
        yield (acct_id, device_id, format_ts(fs), format_ts(ls), cnt)


# ── Name / Address / Country edges ───────────────────────────────


def customer_has_name_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    ts = format_ts(sim_start)
    for person_id in entities.people.ids:
        nm = identity.name_for_person(person_id)
        yield (person_id, nm.id, ts)


def customer_has_address_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    ts = format_ts(sim_start)
    for person_id in entities.people.ids:
        addr = identity.address_for_person(person_id)
        yield (person_id, addr.id, ts)


def customer_associated_with_country_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    ts = format_ts(sim_start)
    for person_id in entities.people.ids:
        yield (person_id, "US", ts)


def account_has_name_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    ts = format_ts(sim_start)
    externals = entities.accounts.externals
    for person_id, acct_ids in entities.accounts.by_person.items():
        nm = identity.name_for_person(person_id)
        for acct_id in acct_ids:
            if is_external(acct_id) or acct_id in externals:
                continue
            yield (acct_id, nm.id, "primary", ts)


def account_has_address_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    ts = format_ts(sim_start)
    externals = entities.accounts.externals
    for person_id, acct_ids in entities.accounts.by_person.items():
        addr = identity.address_for_person(person_id)
        for acct_id in acct_ids:
            if is_external(acct_id) or acct_id in externals:
                continue
            yield (acct_id, addr.id, "mailing", ts)


def account_associated_with_country_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    ts = format_ts(sim_start)
    externals = entities.accounts.externals
    for acct_id in entities.accounts.ids:
        if is_external(acct_id) or acct_id in externals:
            continue
        yield (acct_id, "US", ts)


def address_in_country_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    """Connect every Address vertex to its Country (customers + counterparties + banks)."""
    ts = format_ts(sim_start)
    emitted: set[str] = set()

    for person_id in entities.people.ids:
        addr = identity.address_for_person(person_id)
        if addr.id not in emitted:
            emitted.add(addr.id)
            yield (addr.id, addr.country, ts)

    for cp_id in sorted(entities.accounts.externals):
        addr = identity.address_for_counterparty(cp_id)
        if addr.id not in emitted:
            emitted.add(addr.id)
            yield (addr.id, addr.country, ts)

    for bank_id in sorted(all_bank_ids(entities.accounts.externals)):
        addr = identity.address_for_bank(bank_id)
        if addr.id not in emitted:
            emitted.add(addr.id)
            yield (addr.id, addr.country, ts)


# ── Counterparty identity edges ──────────────────────────────────


def counterparty_has_name_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    ts = format_ts(sim_start)
    for cp_id in sorted(entities.accounts.externals):
        nm = identity.name_for_counterparty(cp_id)
        yield (cp_id, nm.id, ts)


def counterparty_has_address_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    ts = format_ts(sim_start)
    for cp_id in sorted(entities.accounts.externals):
        addr = identity.address_for_counterparty(cp_id)
        yield (cp_id, addr.id, ts)


def counterparty_associated_with_country_rows(
    entities: Entities,
) -> Iterator[Row]:
    for cp_id in sorted(entities.accounts.externals):
        yield (cp_id, "US")


# ── Watchlist edges ──────────────────────────────────────────────


def customer_matches_watchlist_rows(entities: Entities) -> Iterator[Row]:
    people = entities.people
    for person_id in sorted(people.frauds | people.mules):
        yield (person_id, f"WL_{person_id}")


# ── SAR edges ────────────────────────────────────────────────────


def references_rows(sars: list[SarRecord]) -> Iterator[Row]:
    for sar in sars:
        for pid, role in zip(sar.subject_person_ids, sar.subject_roles, strict=True):
            yield (sar.sar_id, pid, role)


def sar_covers_rows(sars: list[SarRecord]) -> Iterator[Row]:
    for sar in sars:
        for aid, amt in zip(sar.covered_account_ids, sar.covered_amounts, strict=True):
            yield (sar.sar_id, aid, amt)


# ── Bank edges ───────────────────────────────────────────────────


def beneficiary_bank_rows(cp_receivers: set[str]) -> Iterator[Row]:
    """Counterparties that actually received funds have a beneficiary bank."""
    for cp_id in sorted(cp_receivers):
        bank_id = counterparty_bank_id(cp_id)
        yield (bank_id, cp_id)


def originator_bank_rows(cp_senders: set[str]) -> Iterator[Row]:
    """Counterparties that actually sent funds have an originator bank."""
    for cp_id in sorted(cp_senders):
        bank_id = counterparty_bank_id(cp_id)
        yield (bank_id, cp_id)


def bank_associated_with_country_rows(entities: Entities) -> Iterator[Row]:
    for bank_id in sorted(all_bank_ids(entities.accounts.externals)):
        yield (bank_id, "US")


def bank_has_address_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    """Connect every Bank vertex (including our own bank) to its Address."""
    ts = format_ts(sim_start)
    for bank_id in sorted(all_bank_ids(entities.accounts.externals)):
        addr = identity.address_for_bank(bank_id)
        yield (bank_id, addr.id, ts)


def bank_has_name_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    """Connect every Bank vertex (including our own bank) to its Name."""
    ts = format_ts(sim_start)
    for bank_id in sorted(all_bank_ids(entities.accounts.externals)):
        nm = identity.name_for_bank(bank_id)
        yield (bank_id, nm.id, ts)


# ── MinHash edges ────────────────────────────────────────────────


def minhash_vertex_sets(
    entities: Entities,
) -> tuple[set[str], set[str], set[str], set[str], set[str]]:
    """Collect all MinHash bucket IDs needed as vertices."""
    nm_set: set[str] = set()
    addr_set: set[str] = set()
    street_set: set[str] = set()
    city_set: set[str] = set()
    state_set: set[str] = set()

    for person_id in entities.people.ids:
        nm = identity.name_for_person(person_id)
        addr = identity.address_for_person(person_id)

        nm_set.update(minhash.name_minhash_ids(nm.first_name, nm.last_name))
        full_addr = f"{addr.street_line1} {addr.city} {addr.state} {addr.postal_code}"
        addr_set.update(minhash.address_minhash_ids(full_addr))
        street_set.update(minhash.street_minhash_ids(addr.street_line1))
        city_set.add(minhash.city_minhash_id(addr.city))
        state_set.add(minhash.state_minhash_id(addr.state))

    for cp_id in sorted(entities.accounts.externals):
        nm = identity.name_for_counterparty(cp_id)
        nm_set.update(minhash.name_minhash_ids(nm.first_name, nm.last_name))

    return nm_set, addr_set, street_set, city_set, state_set


def customer_has_name_minhash_rows(entities: Entities) -> Iterator[Row]:
    for person_id in entities.people.ids:
        nm = identity.name_for_person(person_id)
        for mh_id in minhash.name_minhash_ids(nm.first_name, nm.last_name):
            yield (person_id, mh_id)


def customer_has_address_minhash_rows(entities: Entities) -> Iterator[Row]:
    for person_id in entities.people.ids:
        addr = identity.address_for_person(person_id)
        full = f"{addr.street_line1} {addr.city} {addr.state} {addr.postal_code}"
        for mh_id in minhash.address_minhash_ids(full):
            yield (person_id, mh_id)


def customer_has_address_street_line1_minhash_rows(
    entities: Entities,
) -> Iterator[Row]:
    for person_id in entities.people.ids:
        addr = identity.address_for_person(person_id)
        for mh_id in minhash.street_minhash_ids(addr.street_line1):
            yield (person_id, mh_id)


def customer_has_address_city_minhash_rows(entities: Entities) -> Iterator[Row]:
    for person_id in entities.people.ids:
        addr = identity.address_for_person(person_id)
        yield (person_id, minhash.city_minhash_id(addr.city))


def customer_has_address_state_minhash_rows(entities: Entities) -> Iterator[Row]:
    for person_id in entities.people.ids:
        addr = identity.address_for_person(person_id)
        yield (person_id, minhash.state_minhash_id(addr.state))


def account_has_name_minhash_rows(entities: Entities) -> Iterator[Row]:
    externals = entities.accounts.externals
    for person_id, acct_ids in entities.accounts.by_person.items():
        nm = identity.name_for_person(person_id)
        mh_ids = minhash.name_minhash_ids(nm.first_name, nm.last_name)
        for acct_id in acct_ids:
            if is_external(acct_id) or acct_id in externals:
                continue
            for mh_id in mh_ids:
                yield (acct_id, mh_id)


def counterparty_has_name_minhash_rows(entities: Entities) -> Iterator[Row]:
    for cp_id in sorted(entities.accounts.externals):
        nm = identity.name_for_counterparty(cp_id)
        for mh_id in minhash.name_minhash_ids(nm.first_name, nm.last_name):
            yield (cp_id, mh_id)


# ── Resolves-to (counterparty → customer) ────────────────────────


def resolves_to_rows(
    entities: Entities,
    sim_start: datetime,
) -> Iterator[Row]:
    ts = format_ts(sim_start)
    owner_map = entities.accounts.owner_map
    externals = entities.accounts.externals

    for acct_id in sorted(externals):
        owner = owner_map.get(acct_id)
        if owner is not None:
            yield (acct_id, owner, 0.95, "account_link", ts)
