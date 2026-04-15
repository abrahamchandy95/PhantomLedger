import relationships.social as social_model
import transfers.balances as balances_model
from common.transactions import Transaction
from transfers.factory import TransactionFactory
from transfers.day_to_day import (
    DEFAULT_PARAMETERS,
    CommercialNetwork,
    FinancialObligations,
    PopulationCensus,
    SimulationBounds,
    TransactionEngine,
    build_market,
    simulate,
)

from transfers.legit.blueprints import (
    Blueprint,
    LegitBuildPlan,
    build_paydays_by_person,
)
from transfers.legit.ledger.burdens import monthly_fixed_burden_for_portfolio


def hub_people(
    bp: Blueprint,
    plan: LegitBuildPlan,
) -> set[str]:
    acct_owner = bp.network.accounts.owner_map
    return {
        acct_owner[acct]
        for acct in plan.counterparties.hub_accounts
        if acct in acct_owner
    }


def cards_by_person(
    bp: Blueprint,
) -> dict[str, str]:
    cards = bp.cc_state.cards
    if not bp.cc_state.enabled() or cards is None:
        return {}

    if hasattr(cards, "by_person"):
        return cards.by_person

    return {person_id: card_acct for card_acct, person_id in cards.owner_map.items()}


def fixed_monthly_burden_for_person(
    bp: Blueprint,
    person_id: str,
) -> float:
    portfolios = bp.network.portfolios
    if portfolios is None:
        return 0.0

    return monthly_fixed_burden_for_portfolio(portfolios.get(person_id))


def fixed_monthly_burdens(
    bp: Blueprint,
    plan: LegitBuildPlan,
) -> dict[str, float]:
    if bp.network.portfolios is None:
        return {}

    return {
        person_id: fixed_monthly_burden_for_person(bp, person_id)
        for person_id in plan.persons
    }


def build_population(
    request: Blueprint,
    plan: LegitBuildPlan,
    base_txns: list[Transaction],
) -> PopulationCensus:
    paydays = build_paydays_by_person(
        txns=base_txns,
        owner_map=request.network.accounts.owner_map,
        start_date=plan.start_date,
        days=plan.days,
    )

    return PopulationCensus(
        persons=plan.persons,
        primary_accounts=plan.primary_acct_for_person,
        personas=plan.personas.persona_for_person,
        persona_objects=plan.personas.persona_objects,
        paydays_by_person=paydays,
    )


def build_network(
    request: Blueprint,
    plan: LegitBuildPlan,
) -> CommercialNetwork:
    social = social_model.build(
        request.timeline.rng,
        seed=plan.seed,
        people=plan.persons,
        cfg=request.specs.social,
        hub_people=hub_people(request, plan),
    )

    return CommercialNetwork(
        merchants=request.network.merchants,
        social=social,
    )


def build_bounds(
    request: Blueprint,
    plan: LegitBuildPlan,
) -> SimulationBounds:
    return SimulationBounds(
        start_date=plan.start_date,
        days=plan.days,
        events=request.macro.events,
        merchants_cfg=request.macro.merchants_cfg,
    )


def build_engine(
    request: Blueprint,
    screen_book: balances_model.ClearingHouse | None,
) -> TransactionEngine:
    rng = request.timeline.rng

    return TransactionEngine(
        rng=rng,
        txf=TransactionFactory(rng=rng, infra=request.overrides.infra),
        clearing_house=screen_book,
    )


def build_obligations(
    request: Blueprint,
    plan: LegitBuildPlan,
    base_txns: list[Transaction],
    base_txns_sorted: bool,
) -> FinancialObligations:
    return FinancialObligations(
        base_txns=base_txns,
        base_txns_sorted=base_txns_sorted,
        fixed_monthly_burden=fixed_monthly_burdens(request, plan),
    )


def generate_day_to_day_txns(
    request: Blueprint,
    plan: LegitBuildPlan,
    base_txns: list[Transaction],
    *,
    screen_book: balances_model.ClearingHouse | None = None,
    base_txns_sorted: bool = False,
) -> list[Transaction]:
    population = build_population(request, plan, base_txns)
    network = build_network(request, plan)
    bounds = build_bounds(request, plan)

    market = build_market(
        bounds=bounds,
        census=population,
        network=network,
        params=DEFAULT_PARAMETERS,
        base_seed=plan.seed,
    )
    market = market.__class__(
        start_date=market.start_date,
        days=market.days,
        events=market.events,
        merchants_cfg=market.merchants_cfg,
        population=market.population,
        merchants=market.merchants,
        social=market.social,
        credit_cards=cards_by_person(request),
        person_states=market.person_states,
        paydays=market.paydays,
    )

    engine = build_engine(request, screen_book)
    obligations = build_obligations(request, plan, base_txns, base_txns_sorted)

    return simulate(
        market=market,
        engine=engine,
        obligations=obligations,
        params=DEFAULT_PARAMETERS,
    )
