from collections import defaultdict
from datetime import datetime, timedelta

from common.channels import PAYDAY_INBOUND_CHANNELS
from common.transactions import Transaction


def build_paydays_by_person(
    *,
    txns: list[Transaction],
    owner_map: dict[str, str],
    start_date: datetime,
    days: int,
) -> dict[str, frozenset[int]]:
    """
    Index successful inbound income deposits by recipient and day index.
    """
    if days <= 0 or not txns:
        return {}

    end_excl = start_date + timedelta(days=days)
    out: dict[str, set[int]] = defaultdict(set)

    for txn in txns:
        if txn.channel not in PAYDAY_INBOUND_CHANNELS:
            continue

        person_id = owner_map.get(txn.target)
        if person_id is None:
            continue

        if txn.timestamp < start_date or txn.timestamp >= end_excl:
            continue

        day_index = int((txn.timestamp - start_date).days)
        if 0 <= day_index < days:
            out[person_id].add(day_index)

    return {person_id: frozenset(sorted(day_set)) for person_id, day_set in out.items()}
