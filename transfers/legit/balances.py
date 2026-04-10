from typing import cast

import numpy as np
import numpy.typing as npt

from common.persona_names import SALARIED
import entities.models as models
import transfers.balances as balances_model

from .models import LegitCreditRuntime, LegitInputs, LegitPolicies
from .plans import LegitBuildPlan


def _persona_for_acct_array(
    *,
    accounts_list: list[str],
    acct_owner: dict[str, str],
    persona_for_person: dict[str, str],
    persona_names: list[str],
) -> npt.NDArray[np.int8]:
    persona_id_for_name = {name: int(idx) for idx, name in enumerate(persona_names)}
    salaried_id = int(persona_id_for_name.get(SALARIED, 0))

    out: npt.NDArray[np.int8] = np.empty(len(accounts_list), dtype=np.int8)
    for idx, acct in enumerate(accounts_list):
        person_id = acct_owner.get(acct)
        persona_name = (
            "salaried"
            if person_id is None
            else persona_for_person.get(person_id, SALARIED)
        )
        out[idx] = np.int8(persona_id_for_name.get(persona_name, salaried_id))

    return out


def _apply_credit_card_limits(
    book: balances_model.Ledger,
    credit_cards: models.CreditCards,
) -> None:
    for card, limit_value in credit_cards.limits.items():
        book.set_credit_limit(card, limit_value)


def build_balance_book(
    inputs: LegitInputs,
    policies: LegitPolicies,
    credit_runtime: LegitCreditRuntime,
    plan: LegitBuildPlan,
) -> balances_model.Ledger | None:
    balance_rules = policies.balances
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
        acct_owner=inputs.accounts.owner_map,
        persona_for_person=plan.personas.persona_for_person,
        persona_names=plan.personas.persona_names,
    )

    book = balances_model.initialize(
        balance_rules,
        inputs.rng,
        balances_model.SetupParams(
            accounts=plan.all_accounts,
            account_indices=account_indices,
            hub_indices=hub_indices,
            persona_mapping=cast(npt.NDArray[np.integer], persona_mapping),
            persona_names=plan.personas.persona_names,
        ),
    )

    cards = credit_runtime.cards
    if credit_runtime.enabled() and cards is not None:
        _apply_credit_card_limits(book, cards)

    return book
