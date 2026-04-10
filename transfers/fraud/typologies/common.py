from common.transactions import Transaction
from transfers.factory import TransactionDraft

from ..engine import IllicitContext


def append_bounded_txn(
    ctx: IllicitContext,
    out: list[Transaction],
    budget: int,
    spec: TransactionDraft,
) -> bool:
    if len(out) >= budget:
        return False

    out.append(ctx.execution.txf.make(spec))
    return True
