from dataclasses import dataclass, field

from common.transactions import Transaction
from transfers.balances import BalanceBook


@dataclass(slots=True)
class TxnAccumulator:
    book: BalanceBook | None
    txns: list[Transaction] = field(default_factory=list)

    def append(self, txn: Transaction) -> None:
        if self.book is None:
            self.txns.append(txn)
            return

        if self.book.try_transfer(txn.source, txn.target, float(txn.amount)):
            self.txns.append(txn)

    def extend(self, items: list[Transaction]) -> None:
        for txn in items:
            self.append(txn)
