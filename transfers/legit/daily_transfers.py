import relationships.social as social_model
from common.transactions import Transaction
from transfers.day_to_day import (
    DayToDayBuildRequest,
    DayToDayGenerationRequest,
    build_day_to_day_context,
    generate_day_to_day_superposition,
)

from .models import LegitGenerationRequest
from .plans import LegitBuildPlan


def _hub_people(
    request: LegitGenerationRequest,
    plan: LegitBuildPlan,
) -> set[str]:
    acct_owner = request.inputs.accounts.acct_owner
    return {
        acct_owner[acct]
        for acct in plan.counterparties.hub_accounts
        if acct in acct_owner
    }


def generate_day_to_day_txns(
    request: LegitGenerationRequest,
    plan: LegitBuildPlan,
) -> list[Transaction]:
    inputs = request.inputs
    policies = request.policies
    overrides = request.overrides
    credit_runtime = request.credit_runtime

    social = social_model.build_social_graph(
        inputs.rng,
        seed=plan.seed,
        people=plan.persons,
        policy=policies.social,
        hub_people=_hub_people(request, plan),
    )

    day_ctx = build_day_to_day_context(
        DayToDayBuildRequest(
            events=inputs.events,
            merchants_cfg=inputs.merchants_cfg,
            rng=inputs.rng,
            start_date=plan.start_date,
            days=plan.days,
            persons=plan.persons,
            primary_acct_for_person=plan.primary_acct_for_person,
            persona_for_person=plan.personas.persona_for_person,
            merchants=inputs.merchants,
            social=social,
            base_seed=plan.seed,
        )
    )

    cards = credit_runtime.cards
    card_for_person: dict[str, str] | None = None
    if credit_runtime.enabled() and cards is not None:
        card_for_person = cards.card_for_person

    return generate_day_to_day_superposition(
        DayToDayGenerationRequest(
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
            card_for_person=card_for_person,
        )
    )
