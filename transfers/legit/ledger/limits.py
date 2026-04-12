import numpy as np

from common.math import I64
from common.persona_names import SALARIED
import entities.models as models
import transfers.balances as balances_model

from .burdens import monthly_fixed_burden_for_portfolio
from transfers.legit.blueprints import (
    CCState,
    LegitBuildPlan,
    Specifications,
)
from transfers.legit.blueprints.models import Timeline, Network


def _persona_for_acct_array(
    *,
    accounts_list: list[str],
    acct_owner: dict[str, str],
    persona_for_person: dict[str, str],
    persona_names: list[str],
) -> I64:
    persona_id_for_name = {name: idx for idx, name in enumerate(persona_names)}
    salaried_id = persona_id_for_name.get(SALARIED, 0)

    out = np.empty(len(accounts_list), dtype=np.int64)
    for idx, acct in enumerate(accounts_list):
        person_id = acct_owner.get(acct)
        persona_name = (
            SALARIED
            if person_id is None
            else persona_for_person.get(person_id, SALARIED)
        )
        out[idx] = persona_id_for_name.get(persona_name, salaried_id)

    return out


def _apply_credit_card_limits(
    book: balances_model.Ledger,
    credit_cards: models.CreditCards,
) -> None:
    for card, limit_value in credit_cards.limits.items():
        book.set_credit_limit(card, limit_value)


def _monthly_fixed_burden(
    network: Network,
    person_id: str,
) -> float:
    portfolios = network.portfolios
    if portfolios is None:
        return 0.0

    return monthly_fixed_burden_for_portfolio(portfolios.get(person_id))


def build_balance_book(
    timeline: Timeline,
    network: Network,
    specs: Specifications,
    cc_state: CCState,
    plan: LegitBuildPlan,
) -> balances_model.Ledger | None:
    balance_rules = specs.balances
    if not balance_rules.enable_constraints:
        return None

    account_indices = {acct: idx for idx, acct in enumerate(plan.all_accounts)}
    hub_indices = {
        account_indices[acct]
        for acct in plan.counterparties.hub_accounts
        if acct in account_indices
    }

    persona_mapping = _persona_for_acct_array(
        accounts_list=plan.all_accounts,
        acct_owner=network.accounts.owner_map,
        persona_for_person=plan.personas.persona_for_person,
        persona_names=plan.personas.persona_names,
    )

    book = balances_model.initialize(
        balance_rules,
        timeline.rng,
        balances_model.SetupParams(
            accounts=plan.all_accounts,
            account_indices=account_indices,
            hub_indices=hub_indices,
            persona_mapping=persona_mapping,
            persona_names=plan.personas.persona_names,
        ),
    )

    for person_id, acct in plan.primary_acct_for_person.items():
        idx = account_indices.get(acct)
        if idx is None:
            continue

        burden = _monthly_fixed_burden(network, person_id)
        if burden <= 0.0:
            continue

        book.balances[idx] += 0.35 * burden

    cards = cc_state.cards
    if cc_state.enabled() and cards is not None:
        _apply_credit_card_limits(book, cards)

    return book
