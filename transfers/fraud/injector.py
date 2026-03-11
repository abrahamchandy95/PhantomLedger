from dataclasses import dataclass, field
from datetime import datetime

from common.transactions import Transaction
from transfers.txns import TxnFactory

from .camouflage import inject_camouflage
from .models import FraudInjectionRequest, FraudInjectionResult
from .ring_plan import RingPlan, build_ring_plan
from .run_context import (
    CamouflageContext,
    FraudAccountPools,
    FraudExecution,
    FraudWindow,
    IllicitContext,
)
from .schedule import target_illicit_count
from .typologies import inject_illicit_for_ring


@dataclass(slots=True)
class _FraudInjector:
    request: FraudInjectionRequest
    txf: TxnFactory = field(init=False)
    start_date: datetime = field(init=False)
    days: int = field(init=False)

    def __post_init__(self) -> None:
        self.txf = TxnFactory(
            rng=self.request.runtime.rng,
            infra=self.request.runtime.infra,
        )
        self.start_date = self.request.scenario.window.start_date()
        self.days = int(self.request.scenario.window.days)

    def _execution(self) -> FraudExecution:
        return FraudExecution(
            txf=self.txf,
            rng=self.request.runtime.rng,
        )

    def _window(self) -> FraudWindow:
        return FraudWindow(
            start_date=self.start_date,
            days=self.days,
        )

    def _account_pools(self) -> FraudAccountPools:
        return FraudAccountPools(
            all_accounts=tuple(self.request.scenario.accounts.accounts),
            biller_accounts=self.request.counterparties.biller_accounts,
            employers=self.request.counterparties.employers,
        )

    def _camouflage_context(self) -> CamouflageContext:
        return CamouflageContext(
            execution=self._execution(),
            window=self._window(),
            accounts=self._account_pools(),
            policy=self.request.policies.camouflage,
        )

    def _illicit_context(self) -> IllicitContext:
        pools = self._account_pools()
        return IllicitContext(
            execution=self._execution(),
            window=self._window(),
            biller_accounts=pools.biller_accounts,
            layering_policy=self.request.policies.layering,
            structuring_policy=self.request.policies.structuring,
        )

    @staticmethod
    def _ring_budget(remaining_budget: int, rings_left: int) -> int:
        per_ring = max(1, remaining_budget // max(1, rings_left))
        return min(per_ring, remaining_budget)

    def run(self) -> FraudInjectionResult:
        scenario = self.request.scenario
        runtime = self.request.runtime
        policies = self.request.policies

        policies.validate()

        if scenario.fraud_cfg.num_rings <= 0 or not scenario.people.rings:
            return FraudInjectionResult(txns=list(scenario.base_txns), injected_count=0)

        camouflage_ctx = self._camouflage_context()
        illicit_ctx = self._illicit_context()

        ring_plans: list[RingPlan] = [
            build_ring_plan(ring, scenario.accounts) for ring in scenario.people.rings
        ]

        camouflage: list[Transaction] = []
        for ring_plan in ring_plans:
            camouflage.extend(inject_camouflage(camouflage_ctx, ring_plan))

        target_illicit_n = target_illicit_count(
            target_ratio=float(scenario.fraud_cfg.target_illicit_ratio),
            legit_count=len(scenario.base_txns) + len(camouflage),
        )

        if target_illicit_n <= 0:
            out = list(scenario.base_txns)
            out.extend(camouflage)
            return FraudInjectionResult(txns=out, injected_count=len(camouflage))

        illicit: list[Transaction] = []
        remaining_budget = target_illicit_n

        for ring_index, ring_plan in enumerate(ring_plans):
            if remaining_budget <= 0:
                break

            ring_budget = self._ring_budget(
                remaining_budget,
                len(ring_plans) - ring_index,
            )
            typology = policies.typology.choose(runtime.rng)
            ring_illicit = inject_illicit_for_ring(
                illicit_ctx,
                ring_plan,
                typology,
                ring_budget,
            )
            illicit.extend(ring_illicit)
            remaining_budget -= len(ring_illicit)

        out = list(scenario.base_txns)
        out.extend(camouflage)
        out.extend(illicit)

        return FraudInjectionResult(
            txns=out,
            injected_count=len(camouflage) + len(illicit),
        )


def inject_fraud_transfers(
    request: FraudInjectionRequest,
) -> FraudInjectionResult:
    return _FraudInjector(request).run()
