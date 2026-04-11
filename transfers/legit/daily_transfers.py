import relationships.social as social_model
import transfers.balances as balances_model
from common.transactions import Transaction
from transfers.day_to_day import (
    BuildRequest,
    GenerateRequest,
    build_context,
    generate,
)

from .fixed_burden import monthly_fixed_burden_for_portfolio
from .models import LegitGenerationRequest
from .paydays import build_paydays_by_person
from .plans import LegitBuildPlan


def _hub_people(
    request: LegitGenerationRequest,
    plan: LegitBuildPlan,
) -> set[str]:
    acct_owner = request.inputs.accounts.owner_map
    return {
        acct_owner[acct]
        for acct in plan.counterparties.hub_accounts
        if acct in acct_owner
    }


def _cards_by_person(
    request: LegitGenerationRequest,
) -> dict[str, str] | None:
    cards = request.credit_runtime.cards
    if not request.credit_runtime.enabled() or cards is None:
        return None

    if hasattr(cards, "by_person"):
        return cards.by_person

    return {person_id: card_acct for card_acct, person_id in cards.owner_map.items()}


def _fixed_monthly_burden_for_person(
    request: LegitGenerationRequest,
    person_id: str,
) -> float:
    portfolios = request.inputs.portfolios
    if portfolios is None:
        return 0.0

    return monthly_fixed_burden_for_portfolio(portfolios.get(person_id))


def _fixed_monthly_burdens(
    request: LegitGenerationRequest,
    plan: LegitBuildPlan,
) -> dict[str, float]:
    if request.inputs.portfolios is None:
        return {}

    return {
        person_id: _fixed_monthly_burden_for_person(request, person_id)
        for person_id in plan.persons
    }


def generate_day_to_day_txns(
    request: LegitGenerationRequest,
    plan: LegitBuildPlan,
    base_txns: list[Transaction],
    *,
    screen_book: balances_model.Ledger | None = None,
) -> list[Transaction]:
    inputs = request.inputs
    policies = request.policies
    overrides = request.overrides
    credit_runtime = request.credit_runtime

    social = social_model.build(
        inputs.rng,
        seed=plan.seed,
        people=plan.persons,
        cfg=policies.social,
        hub_people=_hub_people(request, plan),
    )
    paydays_by_person = build_paydays_by_person(
        txns=base_txns,
        owner_map=inputs.accounts.owner_map,
        start_date=plan.start_date,
        days=plan.days,
    )

    day_ctx = build_context(
        BuildRequest(
            events=inputs.events,
            merchants_cfg=inputs.merchants_cfg,
            rng=inputs.rng,
            start_date=plan.start_date,
            days=plan.days,
            persons=plan.persons,
            primary_accounts=plan.primary_acct_for_person,
            personas=plan.personas.persona_for_person,
            persona_objects=plan.personas.persona_objects,
            merchants=inputs.merchants,
            social=social,
            base_seed=plan.seed,
            paydays_by_person=paydays_by_person,
        )
    )

    return generate(
        GenerateRequest(
            events=inputs.events,
            merchants_cfg=inputs.merchants_cfg,
            rng=inputs.rng,
            start_date=plan.start_date,
            days=plan.days,
            ctx=day_ctx,
            infra=overrides.infra,
            credit_policy=(
                policies.credit_issuance if credit_runtime.enabled() else None
            ),
            cards=_cards_by_person(request),
            base_txns=base_txns,
            screen_book=screen_book,
            fixed_monthly_burden=_fixed_monthly_burdens(request, plan),
        )
    )
