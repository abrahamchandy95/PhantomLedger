import transfers.credit_cards as credit_cards_transfer
from common.transactions import Transaction
from transfers.txns import TxnFactory

from .models import LegitGenerationRequest
from .plans import LegitBuildPlan


def generate_credit_lifecycle_txns(
    request: LegitGenerationRequest,
    plan: LegitBuildPlan,
    txf: TxnFactory,
    existing_txns: list[Transaction],
) -> list[Transaction]:
    cards = request.credit_runtime.cards
    if not request.credit_runtime.enabled() or cards is None:
        return []

    return credit_cards_transfer.generate_credit_card_lifecycle_txns(
        request.policies.credit_lifecycle,
        credit_cards_transfer.CreditLifecycleRequest(
            window=request.inputs.window,
            rng=request.inputs.rng,
            base_seed=plan.seed,
            cards=cards,
            existing_txns=existing_txns,
            primary_acct_for_person=plan.primary_acct_for_person,
            issuer_acct=plan.counterparties.issuer_acct,
            txf=txf,
        ),
    )
