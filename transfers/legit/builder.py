from dataclasses import dataclass

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
    LegitPolicies,
    TransfersPayload,
)
from .plans import build_legit_plan
from .recurring import generate_rent_txns, generate_salary_txns
from .self import generate as generate_self_transfer_txns
from .subscriptions import generate as generate_subscription_txns


@dataclass(slots=True)
class LegitTransferBuilder:
    request: LegitGenerationRequest

    @property
    def inputs(self) -> LegitInputs:
        return self.request.inputs

    @property
    def policies(self) -> LegitPolicies:
        return self.request.policies

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
        book = build_balance_book(
            self.inputs,
            self.policies,
            self.credit_runtime,
            plan,
        )
        initial_book = None if book is None else book.copy()

        txf = TransactionFactory(rng=self.inputs.rng, infra=self.overrides.infra)

        # Important: this pass builds a semantic-order candidate stream only.
        # Several downstream generators inspect earlier generated transactions,
        # so we preserve the existing feature order here and defer all balance
        # enforcement to the final chronological replay in
        # pipeline/stages/transfers.py.
        candidate_txns: list[Transaction] = []

        def extend(items: list[Transaction]) -> None:
            candidate_txns.extend(items)

        # Base inbound flows that establish payday cadence.
        # day_to_day inspects candidate_txns to build paydays_by_person, so
        # salary and government deposits must already be present before it runs.
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

        # Payroll-adjacent internal movement. This is not itself a payday
        # channel, but it is part of the base transaction stream that should
        # exist before downstream derived passes run.
        extend(
            split_deposits(
                self.inputs.rng,
                plan,
                txf,
                self.inputs.accounts.by_person,
                candidate_txns,
            )
        )

        # Other base legitimate activity.
        extend(generate_rent_txns(self.inputs, self.policies, plan, txf))
        extend(generate_subscription_txns(self.inputs.rng, plan, txf))
        extend(generate_atm_txns(self.inputs.rng, plan, txf))
        extend(
            generate_self_transfer_txns(
                self.inputs.rng,
                plan,
                txf,
                self.inputs.accounts.by_person,
            )
        )

        # Must run after salary + government because it derives payday timing
        # from the transaction stream passed in as base_txns.
        extend(generate_day_to_day_txns(self.request, plan, candidate_txns))

        # Family transfers are not payday channels, so they do not need to
        # precede day_to_day. Keep them before credit lifecycle so the latter
        # continues to run as a final derived pass over accumulated activity.
        extend(generate_family_txns(self.request, plan, txf))

        # Final derived pass over prior activity.
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
