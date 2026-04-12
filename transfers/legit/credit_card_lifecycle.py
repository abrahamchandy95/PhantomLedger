import transfers.credit_cards as credit_cards_transfer
from common.transactions import Transaction
from transfers.factory import TransactionFactory

from .models import LegitGenerationRequest
from .plans import LegitBuildPlan


def generate_credit_lifecycle_txns(
    request: LegitGenerationRequest,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
    existing_txns: list[Transaction],
) -> list[Transaction]:
    cards = request.credit_runtime.cards
    if not request.credit_runtime.enabled() or cards is None:
        return []

    lifecycle = request.specs.cc_profile

    return credit_cards_transfer.generate(
        lifecycle.terms,
        lifecycle.habits,
        credit_cards_transfer.Request(
            window=request.inputs.window,
            rng=request.inputs.rng,
            base_seed=plan.seed,
            cards=cards,
            txns=existing_txns,
            primary_accounts=plan.primary_acct_for_person,
            issuer_acct=plan.counterparties.issuer_acct,
            txf=txf,
        ),
    )
