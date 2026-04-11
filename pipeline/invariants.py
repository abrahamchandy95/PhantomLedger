from collections.abc import Iterable

from common.transactions import Transaction
from entities import models


def _sample_missing(missing: dict[str, int]) -> str:
    items = sorted(missing.items(), key=lambda item: (-item[1], item[0]))[:10]
    return ", ".join(f"{account_id} ({count})" for account_id, count in items)


def validate_transaction_accounts(
    accounts: models.Accounts,
    txns: Iterable[Transaction],
) -> None:
    """Fail fast if any transaction references an unknown account ID."""
    known_accounts = set(accounts.ids)

    missing_sources: dict[str, int] = {}
    missing_targets: dict[str, int] = {}

    for txn in txns:
        if txn.source not in known_accounts:
            missing_sources[txn.source] = missing_sources.get(txn.source, 0) + 1
        if txn.target not in known_accounts:
            missing_targets[txn.target] = missing_targets.get(txn.target, 0) + 1

    if not missing_sources and not missing_targets:
        return

    parts: list[str] = []
    if missing_sources:
        parts.append(f"missing source accounts: {_sample_missing(missing_sources)}")
    if missing_targets:
        parts.append(f"missing target accounts: {_sample_missing(missing_targets)}")

    detail = "; ".join(parts)
    raise ValueError(
        "Transactions reference accounts that are absent from entities.accounts.ids; "
        + detail
    )
