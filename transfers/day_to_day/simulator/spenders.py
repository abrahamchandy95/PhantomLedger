from dataclasses import dataclass

from ..actors import Spender, build_spender
from ..environment import FinancialObligations, Market


@dataclass(frozen=True, slots=True)
class PreparedSpender:
    person_idx: int
    spender: Spender
    paydays: frozenset[int]
    initial_cash: float
    baseline_cash: float
    fixed_burden: float
    paycheck_sensitivity: float


def prepare_spenders(
    market: Market,
    obligations: FinancialObligations,
) -> list[PreparedSpender]:
    prepared: list[PreparedSpender] = []

    total_people = len(market.population.persons)
    fixed_burden = obligations.fixed_monthly_burden

    for person_idx in range(total_people):
        spender = build_spender(market.population, market.merchants, person_idx)
        if spender is None:
            continue

        initial_cash = float(spender.persona.initial_balance)

        prepared.append(
            PreparedSpender(
                person_idx=person_idx,
                spender=spender,
                paydays=market.paydays[person_idx],
                initial_cash=initial_cash,
                baseline_cash=max(150.0, initial_cash),
                fixed_burden=float(fixed_burden.get(spender.id, 0.0)),
                paycheck_sensitivity=float(spender.persona.paycheck_sensitivity),
            )
        )

    return prepared
