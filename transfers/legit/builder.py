from dataclasses import dataclass
from heapq import merge

import transfers.balances as balances_model

from common.transactions import Transaction
from transfers.factory import TransactionFactory
from transfers.government import generate as generate_government_txns

from .atm import generate as generate_atm_txns
from .balances import build_balance_book
from .credit_card_lifecycle import generate_credit_lifecycle_txns
from .daily_transfers import generate_day_to_day_txns
from .deposit_split import split_deposits
from .family_transfers import generate_family_txns
from .models import (
    LegitCreditRuntime,
    LegitGenerationRequest,
    LegitInputs,
    LegitOverrides,
    Specifications,
    TransfersPayload,
)
from .nonpayroll_income import generate_nonpayroll_income_txns
from .plans import build_legit_plan
from .recurring import generate_rent_txns, generate_salary_txns
from .self import generate as generate_self_transfer_txns
from .subscriptions import generate as generate_subscription_txns


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
class LegitTransferBuilder:
    request: LegitGenerationRequest

    @property
    def inputs(self) -> LegitInputs:
        return self.request.inputs

    @property
    def policies(self) -> Specifications:
        return self.request.specs

    @property
    def overrides(self) -> LegitOverrides:
        return self.request.overrides

    @property
    def credit_runtime(self) -> LegitCreditRuntime:
        return self.request.credit_runtime

    def build(self) -> TransfersPayload:
        if not self.inputs.accounts.ids:
            return TransfersPayload(
                candidate_txns=[],
                hub_accounts=[],
                biller_accounts=[],
                employers=[],
                initial_book=None,
            )

        plan = build_legit_plan(self.inputs, self.overrides)
        initial_book = build_balance_book(
            self.inputs,
            self.policies,
            self.credit_runtime,
            plan,
        )
        screen_book = None if initial_book is None else initial_book.copy()

        txf = TransactionFactory(rng=self.inputs.rng, infra=self.overrides.infra)

        candidate_txns: list[Transaction] = []
        screened_prefix: list[Transaction] = []

        def extend(items: list[Transaction]) -> None:
            nonlocal screened_prefix
            if not items:
                return

            candidate_txns.extend(items)
            screened_prefix = _merge_by_timestamp(screened_prefix, items)

        def reset_screen_book() -> balances_model.Ledger | None:
            if initial_book is None or screen_book is None:
                return None
            screen_book.restore_from(initial_book)
            return screen_book

        extend(generate_salary_txns(self.inputs, self.policies, plan, txf))
        extend(
            generate_government_txns(
                self.inputs.government,
                self.inputs.window,
                self.inputs.rng,
                txf,
                personas=plan.personas.persona_for_person,
                primary_accounts=plan.primary_acct_for_person,
            )
        )
        extend(generate_nonpayroll_income_txns(self.request, plan, txf))
        extend(
            split_deposits(
                self.inputs.rng,
                plan,
                txf,
                self.inputs.accounts.by_person,
                candidate_txns,
            )
        )

        extend(generate_rent_txns(self.inputs, self.policies, plan, txf))

        extend(
            generate_subscription_txns(
                self.inputs.rng,
                plan,
                txf,
                book=reset_screen_book(),
                base_txns=screened_prefix,
                base_txns_sorted=True,
            )
        )

        extend(
            generate_atm_txns(
                self.inputs.rng,
                plan,
                txf,
                book=reset_screen_book(),
                base_txns=screened_prefix,
                base_txns_sorted=True,
            )
        )

        extend(
            generate_self_transfer_txns(
                self.inputs.rng,
                plan,
                txf,
                self.inputs.accounts.by_person,
                book=reset_screen_book(),
                base_txns=screened_prefix,
                base_txns_sorted=True,
            )
        )

        extend(
            generate_day_to_day_txns(
                self.request,
                plan,
                screened_prefix,
                screen_book=reset_screen_book(),
                base_txns_sorted=True,
            )
        )

        extend(generate_family_txns(self.request, plan, txf))

        extend(
            generate_credit_lifecycle_txns(
                self.request,
                plan,
                txf,
                candidate_txns,
            )
        )

        return TransfersPayload(
            candidate_txns=candidate_txns,
            hub_accounts=plan.counterparties.hub_accounts,
            biller_accounts=plan.counterparties.biller_accounts,
            employers=plan.counterparties.employers,
            initial_book=initial_book,
        )
