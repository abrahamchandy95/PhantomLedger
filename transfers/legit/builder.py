from dataclasses import dataclass

from transfers.factory import TransactionFactory
from transfers.government import generate as generate_government_txns

from .accumulator import TxnAccumulator
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
                txns=[],
                hub_accounts=[],
                biller_accounts=[],
                employers=[],
            )

        plan = build_legit_plan(self.inputs, self.overrides)
        book = build_balance_book(
            self.inputs,
            self.policies,
            self.credit_runtime,
            plan,
        )
        txf = TransactionFactory(rng=self.inputs.rng, infra=self.overrides.infra)
        acc = TxnAccumulator(book=book)

        # Base inbound flows that establish payday cadence.
        # day_to_day inspects acc.txns to build paydays_by_person, so salary and
        # government deposits must already be present before it runs.
        acc.extend(generate_salary_txns(self.inputs, self.policies, plan, txf))
        acc.extend(
            generate_government_txns(
                self.inputs.government,
                self.inputs.window,
                self.inputs.rng,
                txf,
                personas=plan.personas.persona_for_person,
                primary_accounts=plan.primary_acct_for_person,
            )
        )

        # Payroll-adjacent internal movement. This is not itself a payday channel,
        # but it is part of the base transaction stream that should exist before
        # downstream derived passes run.
        acc.extend(
            split_deposits(
                self.inputs.rng,
                plan,
                txf,
                self.inputs.accounts.by_person,
                acc.txns,
            )
        )

        # Other base legitimate activity.
        acc.extend(generate_rent_txns(self.inputs, self.policies, plan, txf))
        acc.extend(generate_subscription_txns(self.inputs.rng, plan, txf))
        acc.extend(generate_atm_txns(self.inputs.rng, plan, txf))
        acc.extend(
            generate_self_transfer_txns(
                self.inputs.rng,
                plan,
                txf,
                self.inputs.accounts.by_person,
            )
        )

        # Must run after salary + government because it derives payday timing from
        # the transaction stream passed in as base_txns.
        acc.extend(generate_day_to_day_txns(self.request, plan, acc.txns))

        # Family transfers are not payday channels, so they do not need to precede
        # day_to_day. Keep them before credit lifecycle so the latter continues to
        # run as a final derived pass over accumulated activity.
        acc.extend(generate_family_txns(self.request, plan, txf))

        # Final derived pass over prior activity.
        acc.extend(generate_credit_lifecycle_txns(self.request, plan, txf, acc.txns))

        return TransfersPayload(
            txns=acc.txns,
            hub_accounts=plan.counterparties.hub_accounts,
            biller_accounts=plan.counterparties.biller_accounts,
            employers=plan.counterparties.employers,
        )
