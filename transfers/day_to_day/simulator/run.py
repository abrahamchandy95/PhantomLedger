from common.transactions import Transaction

from ..environment import (
    DEFAULT_PARAMETERS,
    FinancialObligations,
    Market,
    Parameters,
    TransactionEngine,
)
from .driver import Simulator


def simulate(
    market: Market,
    engine: TransactionEngine,
    obligations: FinancialObligations | None = None,
    params: Parameters | None = None,
) -> list[Transaction]:
    resolved_obligations: FinancialObligations
    if obligations is None:
        resolved_obligations = FinancialObligations()
    else:
        resolved_obligations = obligations

    resolved_params: Parameters
    if params is None:
        resolved_params = DEFAULT_PARAMETERS
    else:
        resolved_params = params

    return Simulator(
        market=market,
        engine=engine,
        obligations=resolved_obligations,
        params=resolved_params,
    ).run()
