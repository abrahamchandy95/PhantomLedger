from datetime import timedelta

import numpy as np

from common.random import derive_seed
from common.transactions import Transaction
from transfers.factory import TransactionFactory
from transfers.legit.blueprints import Blueprint, LegitBuildPlan

from .flows import (
    Cycle,
    draft_clients,
    draft_draws,
    draft_investments,
    draft_platforms,
    draft_settlements,
    sort_txns,
)
from .sources import assign_sources


def _quiet_month(
    rng: np.random.Generator,
    *,
    probability: float,
    skip_probability: float,
) -> bool:
    return rng.random() < probability and rng.random() < skip_probability


def build_revenue(
    request: Blueprint,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
) -> list[Transaction]:
    if request.overrides.counterparty_pools is None:
        return []

    end_excl = plan.start_date + timedelta(days=plan.days)
    txns: list[Transaction] = []

    for person_id in plan.persons:
        revenue_plan = assign_sources(request, plan, person_id)
        if revenue_plan is None:
            continue

        profile = revenue_plan.profile
        accounts = revenue_plan.accounts
        sources = revenue_plan.sources

        for month_start in plan.month_starts:
            rng = np.random.default_rng(
                derive_seed(
                    plan.seed,
                    "legit",
                    "nonpayroll_income",
                    person_id,
                    str(month_start.year),
                    str(month_start.month),
                )
            )

            if _quiet_month(
                rng,
                probability=profile.quiet_month.probability,
                skip_probability=profile.quiet_month.skip_probability,
            ):
                continue

            cycle = Cycle(
                rng=rng,
                month_start=month_start,
                window_start=plan.start_date,
                end_excl=end_excl,
                txf=txf,
                txns=txns,
            )

            draft_clients(cycle, profile.client, accounts.revenue_dst, sources.clients)
            draft_platforms(
                cycle, profile.platform, accounts.revenue_dst, sources.platforms
            )
            draft_settlements(
                cycle, profile.settlement, accounts.revenue_dst, sources.processor
            )
            draft_draws(cycle, profile.owner_draw, accounts.personal, sources.draw_src)
            draft_investments(
                cycle,
                profile.investment,
                accounts.personal,
                sources.investment_src,
            )

    sort_txns(txns)
    return txns
