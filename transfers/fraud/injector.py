from dataclasses import dataclass, field
from datetime import datetime

from common.transactions import Transaction
from transfers.factory import TransactionFactory

from .camouflage import generate as generate_camouflage
from .engine import (
    AccountPools,
    ActiveWindow,
    CamouflageContext,
    Execution,
    IllicitContext,
    InjectionInput,
    InjectionOutput,
)
from .rings import Plan, build_plan
from .schedule import calculate_illicit_budget
from .typologies import generate as generate_typologies


@dataclass(slots=True)
class _Injector:
    """Orchestrates the injection of camouflage and illicit transactions for fraud rings."""

    input_data: InjectionInput
    txf: TransactionFactory = field(init=False)
    start_date: datetime = field(init=False)
    days: int = field(init=False)

    def __post_init__(self) -> None:
        self.txf = TransactionFactory(
            rng=self.input_data.runtime.rng,
            infra=self.input_data.runtime.infra,
            ring_infra=self.input_data.runtime.ring_infra,
        )
        self.start_date = self.input_data.scenario.window.start_date
        self.days = int(self.input_data.scenario.window.days)

    def _execution(self) -> Execution:
        return Execution(
            txf=self.txf,
            rng=self.input_data.runtime.rng,
        )

    def _window(self) -> ActiveWindow:
        return ActiveWindow(
            start_date=self.start_date,
            days=self.days,
        )

    def _account_pools(self) -> AccountPools:
        return AccountPools(
            all_accounts=tuple(self.input_data.scenario.accounts.ids),
            biller_accounts=self.input_data.counterparties.biller_accounts,
            employers=self.input_data.counterparties.employers,
        )

    def _camouflage_context(self) -> CamouflageContext:
        return CamouflageContext(
            execution=self._execution(),
            window=self._window(),
            accounts=self._account_pools(),
            rates=self.input_data.params.camouflage,
        )

    def _illicit_context(self) -> IllicitContext:
        pools = self._account_pools()
        return IllicitContext(
            execution=self._execution(),
            window=self._window(),
            biller_accounts=pools.biller_accounts,
            layering_rules=self.input_data.params.layering,
            structuring_rules=self.input_data.params.structuring,
        )

    @staticmethod
    def _ring_budget(remaining_budget: int, rings_left: int) -> int:
        per_ring = max(1, remaining_budget // max(1, rings_left))
        return min(per_ring, remaining_budget)

    def run(self) -> InjectionOutput:
        scenario = self.input_data.scenario
        runtime = self.input_data.runtime
        params = self.input_data.params

        if not scenario.people.rings:
            return InjectionOutput(txns=list(scenario.base_txns), injected_count=0)

        camouflage_ctx = self._camouflage_context()
        illicit_ctx = self._illicit_context()

        ring_plans: list[Plan] = [
            build_plan(ring, scenario.accounts) for ring in scenario.people.rings
        ]

        camo_txns: list[Transaction] = []
        for ring_plan in ring_plans:
            camo_txns.extend(generate_camouflage(camouflage_ctx, ring_plan))

        target_illicit_n = calculate_illicit_budget(
            target_ratio=float(scenario.fraud_cfg.target_illicit_p),
            legit_count=len(scenario.base_txns) + len(camo_txns),
        )

        if target_illicit_n <= 0:
            out = list(scenario.base_txns)
            out.extend(camo_txns)
            return InjectionOutput(txns=out, injected_count=len(camo_txns))

        illicit_txns: list[Transaction] = []
        remaining_budget = target_illicit_n

        for ring_index, ring_plan in enumerate(ring_plans):
            if remaining_budget <= 0:
                break

            ring_budget = self._ring_budget(
                remaining_budget,
                len(ring_plans) - ring_index,
            )

            typology_choice = params.typology.choose(runtime.rng)
            ring_illicit = generate_typologies(
                illicit_ctx,
                ring_plan,
                typology_choice,
                ring_budget,
            )

            illicit_txns.extend(ring_illicit)
            remaining_budget -= len(ring_illicit)

        out = list(scenario.base_txns)
        out.extend(camo_txns)
        out.extend(illicit_txns)

        return InjectionOutput(
            txns=out,
            injected_count=len(camo_txns) + len(illicit_txns),
        )


def inject(input_data: InjectionInput) -> InjectionOutput:
    """Entry point for the fraud injection engine."""
    return _Injector(input_data).run()
