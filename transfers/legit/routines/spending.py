import relationships.social as social_model
import transfers.balances as balances_model
from common.transactions import Transaction
from transfers.day_to_day import (
    BuildRequest,
    GenerateRequest,
    build_context,
    generate,
)

from transfers.legit.blueprints import (
    Blueprint,
    LegitBuildPlan,
    build_paydays_by_person,
)
from transfers.legit.ledger import monthly_fixed_burden_for_portfolio


def _hub_people(
    bp: Blueprint,
    plan: LegitBuildPlan,
) -> set[str]:
    acct_owner = bp.network.accounts.owner_map
    return {
        acct_owner[acct]
        for acct in plan.counterparties.hub_accounts
        if acct in acct_owner
    }


def _cards_by_person(
    bp: Blueprint,
) -> dict[str, str] | None:
    cards = bp.credit_runtime.cards
    if not bp.credit_runtime.enabled() or cards is None:
        return None

    if hasattr(cards, "by_person"):
        return cards.by_person

    return {person_id: card_acct for card_acct, person_id in cards.owner_map.items()}


def _fixed_monthly_burden_for_person(
    bp: Blueprint,
    person_id: str,
) -> float:
    portfolios = bp.network.portfolios
    if portfolios is None:
        return 0.0

    return monthly_fixed_burden_for_portfolio(portfolios.get(person_id))


def _fixed_monthly_burdens(
    bp: Blueprint,
    plan: LegitBuildPlan,
) -> dict[str, float]:
    if bp.network.portfolios is None:
        return {}

    return {
        person_id: _fixed_monthly_burden_for_person(bp, person_id)
        for person_id in plan.persons
    }


def generate_day_to_day_txns(
    request: Blueprint,
    plan: LegitBuildPlan,
    base_txns: list[Transaction],
    *,
    screen_book: balances_model.Ledger | None = None,
    base_txns_sorted: bool = False,
) -> list[Transaction]:
    policies = request.specs
    overrides = request.overrides
    credit_state = request.credit_runtime

    # Mapped rng to Timeline
    social = social_model.build(
        request.timeline.rng,
        seed=plan.seed,
        people=plan.persons,
        cfg=policies.social,
        hub_people=_hub_people(request, plan),
    )

    # Mapped owner_map to Network
    paydays_by_person = build_paydays_by_person(
        txns=base_txns,
        owner_map=request.network.accounts.owner_map,
        start_date=plan.start_date,
        days=plan.days,
    )

    day_ctx = build_context(
        BuildRequest(
            events=request.macro.events,  # Mapped to Macro
            merchants_cfg=request.macro.merchants_cfg,  # Mapped to Macro
            rng=request.timeline.rng,  # Mapped to Timeline
            start_date=plan.start_date,
            days=plan.days,
            persons=plan.persons,
            primary_accounts=plan.primary_acct_for_person,
            personas=plan.personas.persona_for_person,
            persona_objects=plan.personas.persona_objects,
            merchants=request.network.merchants,  # Mapped to Network
            social=social,
            base_seed=plan.seed,
            paydays_by_person=paydays_by_person,
        )
    )

    return generate(
        GenerateRequest(
            events=request.macro.events,  # Mapped to Macro
            merchants_cfg=request.macro.merchants_cfg,  # Mapped to Macro
            rng=request.timeline.rng,  # Mapped to Timeline
            start_date=plan.start_date,
            days=plan.days,
            ctx=day_ctx,
            infra=overrides.infra,
            credit_policy=(
                policies.credit_issuance if credit_state.enabled() else None
            ),
            cards=_cards_by_person(request),
            base_txns=base_txns,
            base_txns_sorted=base_txns_sorted,
            screen_book=screen_book,
            fixed_monthly_burden=_fixed_monthly_burdens(request, plan),
        )
    )
