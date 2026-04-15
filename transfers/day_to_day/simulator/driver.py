from dataclasses import dataclass

from common.progress import maybe_tqdm
from common.transactions import Transaction

from ..channels import build_channel_cdf
from ..dynamics import ContactsView, FavoritesView, PopulationDynamics
from ..environment import (
    FinancialObligations,
    Market,
    Parameters,
    TransactionEngine,
)
from .day import RunPlan, RunState, run_day
from .routing import RoutePolicy
from .spenders import prepare_spenders
from .sync import build_sensitivities, sync_dynamics
from .targets import target_txns


@dataclass(slots=True)
class Simulator:
    market: Market
    engine: TransactionEngine
    obligations: FinancialObligations
    params: Parameters

    def build_plan(self) -> RunPlan:
        market = self.market
        obligations = self.obligations
        params = self.params

        prepared_spenders = prepare_spenders(market, obligations)
        active_spenders = len(prepared_spenders)

        base_txns = (
            obligations.base_txns
            if obligations.base_txns_sorted
            else sorted(obligations.base_txns, key=lambda txn: txn.timestamp)
        )

        total_people = len(market.population.persons)
        per_person_daily_limit = int(market.events.max_per_person_per_day)
        person_limit = None if per_person_daily_limit <= 0 else per_person_daily_limit

        batch = (
            PopulationDynamics.from_persons(market.person_states)
            if total_people >= 500
            else None
        )

        favorites = FavoritesView(
            indices=market.merchants.fav_merchants_idx,
            counts=market.merchants.fav_k,
            cdf=market.merchants.merch_cdf,
            total=len(market.merchants.merchants.ids),
        )
        contacts = ContactsView(
            matrix=market.social.contacts,
            degree=market.social.degree,
            n_people=total_people,
        )

        return RunPlan(
            prepared_spenders=prepared_spenders,
            target_total_txns=target_txns(market, active_spenders),
            total_person_days=active_spenders * market.days,
            base_txns=base_txns,
            sensitivities=build_sensitivities(market),
            batch=batch,
            favorites=favorites,
            contacts=contacts,
            route_policy=RoutePolicy(
                prefer_billers_p=float(market.events.prefer_billers_p),
                max_retries=int(params.picking_rules.max_retries),
            ),
            person_limit=person_limit,
            channel_cdf=build_channel_cdf(market.events, market.merchants_cfg),
        )

    def run(self) -> list[Transaction]:
        if self.market.days <= 0:
            return []

        plan = self.build_plan()
        state = RunState(
            days_since_payday=[365] * len(self.market.population.persons),
        )

        for day_index in maybe_tqdm(
            range(self.market.days),
            desc="day-to-day",
            unit="day",
            leave=False,
        ):
            run_day(self, plan, state, day_index)

        if plan.batch is not None:
            sync_dynamics(plan.batch, self.market.person_states)

        return state.txns
