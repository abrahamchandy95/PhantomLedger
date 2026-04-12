from dataclasses import dataclass
from heapq import merge

import transfers.balances as balances_model

from common.transactions import Transaction
from transfers.factory import TransactionFactory
from transfers.government import generate as generate_government_txns

from .posting import merge_replay_sorted
from .limits import build_balance_book

from transfers.legit.blueprints import (
    CCState,
    Blueprint,
    Overrides,
    Specifications,
    TransfersPayload,
    build_legit_plan,
)

from transfers.legit.inflows import (
    generate_nonpayroll_income_txns,
    generate_rent_txns,
    generate_salary_txns,
)

from transfers.legit.routines import (
    generate_atm_txns,
    generate_credit_lifecycle_txns,
    generate_day_to_day_txns,
    generate_family_txns,
    generate_internal_txns,
    generate_subscription_txns,
    split_deposits,
)


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
    request: Blueprint

    @property
    def timeline(self):
        return self.request.timeline

    @property
    def network(self):
        return self.request.network

    @property
    def macro(self):
        return self.request.macro

    @property
    def specs(self) -> Specifications:
        return self.request.specs

    @property
    def overrides(self) -> Overrides:
        return self.request.overrides

    @property
    def cc_state(self) -> CCState:
        return self.request.cc_state

    def build(self) -> TransfersPayload:
        if not self.network.accounts.ids:
            return TransfersPayload(
                candidate_txns=[],
                hub_accounts=[],
                biller_accounts=[],
                employers=[],
                initial_book=None,
            )

        # Passes the whole Blueprint (request) instead of inputs
        plan = build_legit_plan(self.timeline, self.network, self.macro, self.overrides)
        initial_book = build_balance_book(
            self.timeline,
            self.network,
            self.specs,
            self.cc_state,
            plan,
        )
        screen_book = None if initial_book is None else initial_book.copy()

        # Uses timeline instead of inputs
        txf = TransactionFactory(rng=self.timeline.rng, infra=self.overrides.infra)

        candidate_txns: list[Transaction] = []
        screened_prefix: list[Transaction] = []
        replay_sorted_txns: list[Transaction] = []

        def extend(items: list[Transaction]) -> None:
            nonlocal screened_prefix, replay_sorted_txns
            if not items:
                return

            candidate_txns.extend(items)
            screened_prefix = _merge_by_timestamp(screened_prefix, items)
            replay_sorted_txns = merge_replay_sorted(replay_sorted_txns, items)

        def reset_screen_book() -> balances_model.Ledger | None:
            if initial_book is None or screen_book is None:
                return None
            screen_book.restore_from(initial_book)
            return screen_book

        # --- Inbound Money ---
        # Passes the whole Blueprint (request) instead of inputs
        extend(generate_salary_txns(self.timeline, self.specs, plan, txf))
        extend(
            generate_government_txns(
                self.macro.government,  # Extracted from macro
                self.timeline.window,  # Extracted from timeline
                self.timeline.rng,  # Extracted from timeline
                txf,
                personas=plan.personas.persona_for_person,
                primary_accounts=plan.primary_acct_for_person,
            )
        )
        extend(generate_nonpayroll_income_txns(self.request, plan, txf))

        # --- Consumer Routines ---
        extend(
            split_deposits(
                self.timeline.rng,  # Extracted from timeline
                plan,
                txf,
                self.network.accounts.by_person,  # Extracted from network
                candidate_txns,
            )
        )

        # Passes the whole Blueprint (request) instead of inputs
        extend(generate_rent_txns(self.timeline, self.network, self.specs, plan, txf))

        extend(
            generate_subscription_txns(
                self.timeline.rng,  # Extracted from timeline
                plan,
                txf,
                book=reset_screen_book(),
                base_txns=screened_prefix,
                base_txns_sorted=True,
            )
        )

        extend(
            generate_atm_txns(
                self.timeline.rng,  # Extracted from timeline
                plan,
                txf,
                book=reset_screen_book(),
                base_txns=screened_prefix,
                base_txns_sorted=True,
            )
        )

        extend(
            generate_internal_txns(
                self.timeline.rng,  # Extracted from timeline
                plan,
                txf,
                self.network.accounts.by_person,  # Extracted from network
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
            replay_sorted_txns=replay_sorted_txns,
        )
