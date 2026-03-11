from common.transactions import Transaction
from transfers.txns import TxnSpec

from .run_context import IllicitContext


def append_bounded_txn(
    ctx: IllicitContext,
    out: list[Transaction],
    budget: int,
    spec: TxnSpec,
) -> bool:
    if len(out) >= budget:
        return False

    out.append(ctx.execution.txf.make(spec))
    return True
