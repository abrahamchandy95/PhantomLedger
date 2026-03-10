from typing import cast

import numpy as np
import numpy.typing as npt

import entities.credit_cards as credit_cards_entity
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
    salaried_id = int(persona_id_for_name.get("salaried", 0))

    out: npt.NDArray[np.int8] = np.empty(len(accounts_list), dtype=np.int8)
    for idx, acct in enumerate(accounts_list):
        person_id = acct_owner.get(acct)
        persona_name = (
            "salaried"
            if person_id is None
            else persona_for_person.get(person_id, "salaried")
        )
        out[idx] = np.int8(persona_id_for_name.get(persona_name, salaried_id))

    return out


def _apply_credit_card_limits(
    book: balances_model.BalanceBook,
    credit_cards: credit_cards_entity.CreditCardData,
) -> None:
    for card, limit_value in credit_cards.limit_by_card.items():
        book.set_credit_limit(card, limit_value)


def build_balance_book(
    inputs: LegitInputs,
    policies: LegitPolicies,
    credit_runtime: LegitCreditRuntime,
    plan: LegitBuildPlan,
) -> balances_model.BalanceBook | None:
    balance_policy = policies.balances
    if not balance_policy.enable_balance_constraints:
        return None

    acct_index = {acct: idx for idx, acct in enumerate(plan.all_accounts)}
    hub_set_idx = {
        acct_index[acct]
        for acct in plan.counterparties.hub_accounts
        if acct in acct_index
    }

    persona_for_acct = _persona_for_acct_array(
        accounts_list=plan.all_accounts,
        acct_owner=inputs.accounts.acct_owner,
        persona_for_person=plan.personas.persona_for_person,
        persona_names=plan.personas.persona_names,
    )

    book = balances_model.init_balances(
        balance_policy,
        inputs.rng,
        balances_model.BalanceInitRequest(
            accounts=plan.all_accounts,
            acct_index=acct_index,
            hub_set_idx=hub_set_idx,
            persona_for_acct=cast(npt.NDArray[np.integer], persona_for_acct),
            persona_names=plan.personas.persona_names,
        ),
    )

    cards = credit_runtime.cards
    if credit_runtime.enabled() and cards is not None:
        _apply_credit_card_limits(book, cards)

    return book
