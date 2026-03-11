from dataclasses import dataclass

from transfers.txns import TxnFactory

from .accumulator import TxnAccumulator
from .balances import build_balance_book
from .credit_card_lifecycle import generate_credit_lifecycle_txns
from .daily_transfers import generate_day_to_day_txns
from .family_transfers import generate_family_txns
from .models import (
    LegitCreditRuntime,
    LegitGenerationRequest,
    LegitInputs,
    LegitOverrides,
    LegitPolicies,
    LegitTransfers,
)
from .plans import build_legit_plan
from .recurring import generate_rent_txns, generate_salary_txns


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

    def build(self) -> LegitTransfers:
        self.policies.validate()

        if not self.inputs.accounts.accounts:
            return LegitTransfers(
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
        txf = TxnFactory(rng=self.inputs.rng, infra=self.overrides.infra)
        acc = TxnAccumulator(book=book)

        acc.extend(generate_salary_txns(self.inputs, self.policies, plan, txf))
        acc.extend(generate_rent_txns(self.inputs, self.policies, plan, txf))
        acc.extend(generate_day_to_day_txns(self.request, plan))
        acc.extend(generate_family_txns(self.request, plan, txf))
        acc.extend(generate_credit_lifecycle_txns(self.request, plan, txf, acc.txns))

        return LegitTransfers(
            txns=acc.txns,
            hub_accounts=plan.counterparties.hub_accounts,
            biller_accounts=plan.counterparties.biller_accounts,
            employers=plan.counterparties.employers,
        )
