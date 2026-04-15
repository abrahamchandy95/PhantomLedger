from dataclasses import dataclass

import numpy as np

from common.business_accounts import (
    brokerage_custody_account_id,
    business_operating_account_id,
)
from common.persona_names import FREELANCER, HNW, SMALLBIZ
from common.random import derive_seed
from entities.counterparties import Pools as CounterpartyPools
from transfers.legit.blueprints import Blueprint, LegitBuildPlan

from .catalog import REVENUE_ARCHETYPES
from .draw import choice_k, pick_one
from .profiles import (
    CounterpartyRevenueProfile,
    RevenueFlowProfile,
    RevenuePersonaProfile,
)


@dataclass(frozen=True, slots=True)
class RevenueAccounts:
    personal: str
    revenue_dst: str
    business: str | None
    brokerage: str | None


@dataclass(frozen=True, slots=True)
class RevenueSources:
    clients: tuple[str, ...] = ()
    platforms: tuple[str, ...] = ()
    processor: str | None = None
    draw_src: str | None = None
    investment_src: str | None = None

    def any(self) -> bool:
        return bool(
            self.clients
            or self.platforms
            or self.processor
            or self.draw_src
            or self.investment_src
        )


@dataclass(frozen=True, slots=True)
class RevenuePlan:
    person_id: str
    persona: str
    profile: RevenuePersonaProfile
    accounts: RevenueAccounts
    sources: RevenueSources


def owned_business(
    accounts_by_person: dict[str, list[str]],
    person_id: str,
) -> str | None:
    expected = business_operating_account_id(person_id)
    person_accounts = accounts_by_person.get(person_id, [])
    if expected in person_accounts:
        return expected
    return None


def owned_brokerage(
    accounts_by_person: dict[str, list[str]],
    person_id: str,
) -> str | None:
    expected = brokerage_custody_account_id(person_id)
    person_accounts = accounts_by_person.get(person_id, [])
    if expected in person_accounts:
        return expected
    return None


def _draw_counterparty_sources(
    rng: np.random.Generator,
    pool: list[str],
    profile: CounterpartyRevenueProfile,
) -> tuple[str, ...]:
    if profile.active_p <= 0.0 or not pool:
        return ()
    if rng.random() >= profile.active_p:
        return ()
    return choice_k(
        rng,
        pool,
        low=profile.counterparties_min,
        high=profile.counterparties_max,
    )


def _draw_external_source(
    rng: np.random.Generator,
    pool: list[str],
    profile: RevenueFlowProfile,
) -> str | None:
    if profile.active_p <= 0.0 or not pool:
        return None
    if rng.random() >= profile.active_p:
        return None
    return pick_one(rng, pool)


def _fallback_sources(
    *,
    persona: str,
    rng: np.random.Generator,
    pools: CounterpartyPools,
    accounts: RevenueAccounts,
    sources: RevenueSources,
) -> RevenueSources:
    if sources.any():
        return sources

    clients = sources.clients
    draw_src = sources.draw_src
    investment_src = sources.investment_src

    if persona == FREELANCER and pools.client_payer_ids:
        clients = choice_k(rng, pools.client_payer_ids, low=1, high=2)
    elif persona == SMALLBIZ and accounts.business is None and pools.owner_business_ids:
        draw_src = pick_one(rng, pools.owner_business_ids)
    elif persona == HNW:
        if accounts.brokerage is not None:
            investment_src = accounts.brokerage
        elif pools.brokerage_ids:
            investment_src = pick_one(rng, pools.brokerage_ids)

    return RevenueSources(
        clients=clients,
        platforms=sources.platforms,
        processor=sources.processor,
        draw_src=draw_src,
        investment_src=investment_src,
    )


def assign_sources(
    request: Blueprint,
    plan: LegitBuildPlan,
    person_id: str,
) -> RevenuePlan | None:
    pools = request.overrides.counterparty_pools
    if pools is None:
        return None

    persona = plan.personas.persona_for_person.get(person_id)
    if persona is None:
        return None

    profile = REVENUE_ARCHETYPES.get(persona)
    if profile is None:
        return None

    personal = plan.primary_acct_for_person.get(person_id)
    if personal is None or personal in plan.counterparties.hub_set:
        return None

    accounts_by_person = request.network.accounts.by_person
    business = owned_business(accounts_by_person, person_id)
    brokerage = owned_brokerage(accounts_by_person, person_id)

    revenue_dst = (
        business
        if persona in {FREELANCER, SMALLBIZ} and business is not None
        else personal
    )
    accounts = RevenueAccounts(
        personal=personal,
        revenue_dst=revenue_dst,
        business=business,
        brokerage=brokerage,
    )

    rng = np.random.default_rng(
        derive_seed(plan.seed, "legit", "nonpayroll_income", person_id)
    )

    sources = RevenueSources(
        clients=_draw_counterparty_sources(rng, pools.client_payer_ids, profile.client),
        platforms=_draw_counterparty_sources(rng, pools.platform_ids, profile.platform),
        processor=_draw_external_source(rng, pools.processor_ids, profile.settlement),
        draw_src=(
            business
            if business is not None
            else _draw_external_source(
                rng, pools.owner_business_ids, profile.owner_draw
            )
        ),
        investment_src=(
            brokerage
            if brokerage is not None
            else _draw_external_source(rng, pools.brokerage_ids, profile.investment)
        ),
    )

    sources = _fallback_sources(
        persona=persona,
        rng=rng,
        pools=pools,
        accounts=accounts,
        sources=sources,
    )

    return RevenuePlan(
        person_id=person_id,
        persona=persona,
        profile=profile,
        accounts=accounts,
        sources=sources,
    )
