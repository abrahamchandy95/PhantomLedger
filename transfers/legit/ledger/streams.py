from dataclasses import dataclass, field
from heapq import merge

from common.transactions import Transaction

from .posting import merge_replay_sorted


def _merge_by_timestamp(
    existing: list[Transaction],
    new_items: list[Transaction],
) -> list[Transaction]:
    if not new_items:
        return existing

    new_sorted = sorted(new_items, key=lambda txn: txn.timestamp)
    if not existing:
        return new_sorted

    return list(merge(existing, new_sorted, key=lambda txn: txn.timestamp))


@dataclass(slots=True)
class TxnStreams:
    candidates: list[Transaction] = field(default_factory=list)
    screened: list[Transaction] = field(default_factory=list)
    replay_ready: list[Transaction] = field(default_factory=list)

    def add(self, items: list[Transaction]) -> None:
        if not items:
            return

        self.candidates.extend(items)
        self.screened = _merge_by_timestamp(self.screened, items)
        self.replay_ready = merge_replay_sorted(self.replay_ready, items)
