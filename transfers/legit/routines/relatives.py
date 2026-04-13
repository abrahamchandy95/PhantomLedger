from relationships.family import build as build_family
from common.transactions import Transaction
from transfers.family import Runtime, generate
from transfers.family.engine import GraphConfig, TransferConfig
from transfers.factory import TransactionFactory

from transfers.legit.blueprints import Blueprint, LegitBuildPlan


def generate_family_txns(
    bp: Blueprint,
    graph_cfg: GraphConfig,
    transfer_cfg: TransferConfig,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
) -> list[Transaction]:
    family = build_family(
        graph_cfg.households,
        graph_cfg.dependents,
        graph_cfg.retiree_support,
        base_seed=plan.seed,
        people=plan.persons,
        persona_map=plan.personas.persona_for_person,
    )

    return generate(
        Runtime(
            window=bp.timeline.window,
            rng=bp.timeline.rng,
            base_seed=plan.seed,
            family=family,
            personas=plan.personas.persona_for_person,
            persona_objects=plan.personas.persona_objects,
            primary_accounts=plan.primary_acct_for_person,
            merchants=bp.network.merchants,
            txf=txf,
        ),
        transfer_cfg,
    )
