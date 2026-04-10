from collections import Counter
from collections.abc import Iterable
from dataclasses import dataclass, field

from common.transactions import Transaction
from transfers.balances import Ledger


@dataclass(slots=True)
class ChronoReplayAccumulator:
    """
    Chronological replay utility used for authoritative balance gating.

    This should not be used inside semantic generators. It exists to replay a
    fully assembled candidate stream from a pristine starting ledger and record
    accepted transactions plus structured drop reasons.
    """

    book: Ledger | None
    txns: list[Transaction] = field(default_factory=list)
    drop_counts: Counter[str] = field(default_factory=Counter)

    def append(self, txn: Transaction) -> bool:
        if self.book is None:
            self.txns.append(txn)
            return True

        decision = self.book.try_transfer_with_reason(
            txn.source,
            txn.target,
            float(txn.amount),
        )
        if decision.accepted:
            self.txns.append(txn)
            return True

        if decision.reason is not None:
            self.drop_counts[decision.reason] += 1
        return False

    def extend(self, items: Iterable[Transaction]) -> None:
        for txn in items:
            _ = self.append(txn)


# Backward-compat alias. Safe to delete later once all imports are updated.
TxnAccumulator = ChronoReplayAccumulator
