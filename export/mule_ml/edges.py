from collections.abc import Iterator
from datetime import datetime

from common.timeline import format_ts
from pipeline.state import Entities, Infra, Transfers
from ..csv_io import Row


DEVICE_HEADER: tuple[str, ...] = (
    "accountID",
    "device_id",
    "txn_count",
    "first_seen",
    "last_seen",
)

IP_HEADER: tuple[str, ...] = (
    "accountID",
    "ip_address",
    "txn_count",
    "first_seen",
    "last_seen",
)


def _merge_txn_pairs(
    pair_count: dict[tuple[str, str], int],
    pair_first: dict[tuple[str, str], str],
    pair_last: dict[tuple[str, str], str],
    transfers: Transfers,
    *,
    field: str,
) -> None:
    """
    Phase 1: Aggregate (src_acct, infra_item) pairs from the transaction log.
    """
    for txn in transfers.final_txns:
        field_val: str | None = (
            txn.device_id if field == "device_id" else txn.ip_address
        )
        if not field_val:
            continue
        key = (txn.source, field_val)
        ts = format_ts(txn.timestamp)

        pair_count[key] = pair_count.get(key, 0) + 1

        if key not in pair_first or ts < pair_first[key]:
            pair_first[key] = ts
        if key not in pair_last or ts > pair_last[key]:
            pair_last[key] = ts


def _merge_infra_baseline(
    pair_count: dict[tuple[str, str], int],
    pair_first: dict[tuple[str, str], str],
    pair_last: dict[tuple[str, str], str],
    entities: Entities,
    *,
    records: list[tuple[str, str, datetime, datetime]],
) -> None:
    """
    Phase 2: Merge infra registry baseline records.

    Records are pre-extracted as (person_id, item_id, first_seen, last_seen)
    so this function has no unknown types.
    """
    for person_id, item_id, first_seen, last_seen in records:
        owner_accounts = entities.accounts.by_person.get(person_id, [])
        first = format_ts(first_seen)
        last = format_ts(last_seen)

        for acct in owner_accounts:
            key = (acct, item_id)
            if key not in pair_count:
                pair_count[key] = 0
                pair_first[key] = first
                pair_last[key] = last
            else:
                if first < pair_first[key]:
                    pair_first[key] = first
                if last > pair_last[key]:
                    pair_last[key] = last


def _emit_sorted(
    pair_count: dict[tuple[str, str], int],
    pair_first: dict[tuple[str, str], str],
    pair_last: dict[tuple[str, str], str],
) -> Iterator[Row]:
    """Phase 3: Yield sorted output rows."""
    for (acct, item), count in sorted(pair_count.items()):
        yield (acct, item, count, pair_first[(acct, item)], pair_last[(acct, item)])


def device_rows(
    entities: Entities, infra: Infra, transfers: Transfers
) -> Iterator[Row]:
    pair_count: dict[tuple[str, str], int] = {}
    pair_first: dict[tuple[str, str], str] = {}
    pair_last: dict[tuple[str, str], str] = {}

    _merge_txn_pairs(pair_count, pair_first, pair_last, transfers, field="device_id")

    device_records: list[tuple[str, str, datetime, datetime]] = [
        (rec.person_id, rec.device_id, rec.first_seen, rec.last_seen)
        for rec in infra.devices.has_used
    ]
    _merge_infra_baseline(
        pair_count, pair_first, pair_last, entities, records=device_records
    )

    return _emit_sorted(pair_count, pair_first, pair_last)


def ip_rows(entities: Entities, infra: Infra, transfers: Transfers) -> Iterator[Row]:
    pair_count: dict[tuple[str, str], int] = {}
    pair_first: dict[tuple[str, str], str] = {}
    pair_last: dict[tuple[str, str], str] = {}

    _merge_txn_pairs(pair_count, pair_first, pair_last, transfers, field="ip_address")

    ip_records: list[tuple[str, str, datetime, datetime]] = [
        (rec.person_id, rec.ip_address, rec.first_seen, rec.last_seen)
        for rec in infra.ips.has_ip
    ]
    _merge_infra_baseline(
        pair_count, pair_first, pair_last, entities, records=ip_records
    )

    return _emit_sorted(pair_count, pair_first, pair_last)
