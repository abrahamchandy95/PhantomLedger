from relationships.family import generate_family
from common.transactions import Transaction
from transfers.family import FamilyTransferRequest, generate_family_transfers
from transfers.txns import TxnFactory

from .models import LegitGenerationRequest
from .plans import LegitBuildPlan


def generate_family_txns(
    request: LegitGenerationRequest,
    plan: LegitBuildPlan,
    txf: TxnFactory,
) -> list[Transaction]:
    family_cfg = request.overrides.family_cfg
    if family_cfg is None:
        return []

    family = generate_family(
        family_cfg,
        request.inputs.rng,
        base_seed=plan.seed,
        persons=plan.persons,
        persona_for_person=plan.personas.persona_for_person,
    )

    return generate_family_transfers(
        FamilyTransferRequest(
            window=request.inputs.window,
            family_cfg=family_cfg,
            rng=request.inputs.rng,
            base_seed=plan.seed,
            family=family,
            persona_for_person=plan.personas.persona_for_person,
            primary_acct_for_person=plan.primary_acct_for_person,
            merchants=request.inputs.merchants,
            txf=txf,
        )
    )
