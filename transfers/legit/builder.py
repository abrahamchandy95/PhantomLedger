from dataclasses import dataclass, field

from common.types import Txn
from relationships.family import generate_family
import relationships.social as social_model
from transfers.balances import BalanceBook
import transfers.credit_cards as credit_cards_transfer
from transfers.day_to_day import (
    DayToDayBuildRequest,
    DayToDayGenerationRequest,
    build_day_to_day_context,
    generate_day_to_day_superposition,
)
from transfers.family import generate_family_transfers, FamilyTransferRequest
from transfers.txns import TxnFactory

from .balances import build_balance_book
from .models import (
    LegitCreditRuntime,
    LegitGenerationRequest,
    LegitInputs,
    LegitOverrides,
    LegitPolicies,
    LegitTransfers,
)
from .plans import LegitBuildPlan, build_legit_plan
from .recurring import generate_rent_txns, generate_salary_txns


@dataclass(slots=True)
class _TxnAccumulator:
    book: BalanceBook | None
    txns: list[Txn] = field(default_factory=list)

    def append(self, txn: Txn) -> None:
        if self.book is None:
            self.txns.append(txn)
            return

        if self.book.try_transfer(txn.src_acct, txn.dst_acct, float(txn.amount)):
            self.txns.append(txn)

    def extend(self, items: list[Txn]) -> None:
        for txn in items:
            self.append(txn)


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

        all_accounts = self.inputs.accounts.accounts
        if not all_accounts:
            return LegitTransfers(
                txns=[],
                hub_accounts=[],
                biller_accounts=[],
                employers=[],
            )

        txf = TxnFactory(rng=self.inputs.rng, infra=self.overrides.infra)
        plan = build_legit_plan(self.inputs, self.overrides)
        book = build_balance_book(
            self.inputs,
            self.policies,
            self.credit_runtime,
            plan,
        )
        acc = _TxnAccumulator(book=book)

        acc.extend(generate_salary_txns(self.inputs, self.policies, plan, txf))
        acc.extend(generate_rent_txns(self.inputs, self.policies, plan, txf))
        acc.extend(self._generate_day_to_day_txns(plan))
        acc.extend(self._generate_family_txns(plan, txf))
        acc.extend(self._generate_credit_lifecycle_txns(plan, txf, acc.txns))

        return LegitTransfers(
            txns=acc.txns,
            hub_accounts=plan.counterparties.hub_accounts,
            biller_accounts=plan.counterparties.biller_accounts,
            employers=plan.counterparties.employers,
        )

    def _generate_day_to_day_txns(
        self,
        plan: LegitBuildPlan,
    ) -> list[Txn]:
        hub_people = {
            self.inputs.accounts.acct_owner[acct]
            for acct in plan.counterparties.hub_accounts
            if acct in self.inputs.accounts.acct_owner
        }

        social = social_model.build_social_graph(
            self.inputs.rng,
            seed=plan.seed,
            people=plan.persons,
            policy=self.policies.social,
            hub_people=hub_people,
        )

        day_ctx = build_day_to_day_context(
            DayToDayBuildRequest(
                events=self.inputs.events,
                merchants_cfg=self.inputs.merchants_cfg,
                rng=self.inputs.rng,
                start_date=plan.start_date,
                days=plan.days,
                persons=plan.persons,
                primary_acct_for_person=plan.primary_acct_for_person,
                persona_for_person=plan.personas.persona_for_person,
                merchants=self.inputs.merchants,
                social=social,
                base_seed=plan.seed,
            )
        )

        cards = self.credit_runtime.cards
        card_for_person: dict[str, str] | None = None
        if self.credit_runtime.enabled() and cards is not None:
            card_for_person = cards.card_for_person

        return generate_day_to_day_superposition(
            DayToDayGenerationRequest(
                events=self.inputs.events,
                merchants_cfg=self.inputs.merchants_cfg,
                rng=self.inputs.rng,
                start_date=plan.start_date,
                days=plan.days,
                ctx=day_ctx,
                infra=self.overrides.infra,
                credit_policy=(
                    self.policies.credit_issuance
                    if self.credit_runtime.enabled()
                    else None
                ),
                card_for_person=card_for_person,
            )
        )

    def _generate_family_txns(
        self,
        plan: LegitBuildPlan,
        txf: TxnFactory,
    ) -> list[Txn]:
        family_cfg = self.overrides.family_cfg
        if family_cfg is None:
            return []

        family = generate_family(
            family_cfg,
            self.inputs.rng,
            base_seed=plan.seed,
            persons=plan.persons,
            persona_for_person=plan.personas.persona_for_person,
        )

        return generate_family_transfers(
            FamilyTransferRequest(
                window=self.inputs.window,
                family_cfg=family_cfg,
                rng=self.inputs.rng,
                base_seed=plan.seed,
                family=family,
                persona_for_person=plan.personas.persona_for_person,
                primary_acct_for_person=plan.primary_acct_for_person,
                merchants=self.inputs.merchants,
                txf=txf,
            )
        )

    def _generate_credit_lifecycle_txns(
        self,
        plan: LegitBuildPlan,
        txf: TxnFactory,
        existing_txns: list[Txn],
    ) -> list[Txn]:
        cards = self.credit_runtime.cards
        if not self.credit_runtime.enabled() or cards is None:
            return []

        return credit_cards_transfer.generate_credit_card_lifecycle_txns(
            self.policies.credit_lifecycle,
            credit_cards_transfer.CreditLifecycleRequest(
                window=self.inputs.window,
                rng=self.inputs.rng,
                base_seed=plan.seed,
                cards=cards,
                existing_txns=existing_txns,
                primary_acct_for_person=plan.primary_acct_for_person,
                issuer_acct=plan.counterparties.issuer_acct,
                txf=txf,
            ),
        )
