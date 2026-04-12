from relationships.family import build as build_family
from common.transactions import Transaction
from transfers.family import GenerateRequest, generate
from transfers.factory import TransactionFactory

from transfers.legit.blueprints import Blueprint, LegitBuildPlan


def generate_family_txns(
    request: Blueprint,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
) -> list[Transaction]:
    family_cfg = request.overrides.family_cfg
    if family_cfg is None:
        return []

    family = build_family(
        family_cfg,
        base_seed=plan.seed,
        people=plan.persons,
        persona_map=plan.personas.persona_for_person,
    )

    return generate(
        GenerateRequest(
            window=request.timeline.window,
            params=family_cfg,
            rng=request.timeline.rng,
            base_seed=plan.seed,
            family=family,
            personas=plan.personas.persona_for_person,
            persona_objects=plan.personas.persona_objects,
            primary_accounts=plan.primary_acct_for_person,
            merchants=request.network.merchants,
            txf=txf,
        )
    )
