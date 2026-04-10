from typing import cast

from common.channels import CARD_PURCHASE, MERCHANT
from common.math import cdf_pick
from transfers.factory import TransactionDraft

from .engine import GenerateRequest
from .state import Event


def choose_merchant(request: GenerateRequest, event: Event) -> int:
    """Returns the merchant index so callers can look up category."""
    merch = request.ctx.merchant
    spender = event.spender
    idx = event.person_idx

    exploring = request.rng.coin(event.explore_p)

    if not exploring and spender.fav_k > 0:
        fav_idx = request.rng.int(0, spender.fav_k)
        merchant_idx = int(cast(int, merch.fav_merchants_idx[idx, fav_idx]))

    else:
        merchant_idx = cdf_pick(merch.merch_cdf, request.rng.float())

        if spender.fav_k > 0:
            favorites = set(
                cast(list[int], merch.fav_merchants_idx[idx, : spender.fav_k].tolist())
            )
            tries = 0
            retry_limit = int(request.params.merchant_retry_limit)

            while merchant_idx in favorites and tries < retry_limit:
                merchant_idx = cdf_pick(merch.merch_cdf, request.rng.float())
                tries += 1

    return merchant_idx


def determine_payment_method(
    request: GenerateRequest,
    event: Event,
) -> tuple[str, str]:
    deposit_acct = event.spender.deposit_acct

    if not request.cards:
        return deposit_acct, MERCHANT

    card_acct = request.cards.get(event.spender.id)
    if not card_acct:
        return deposit_acct, MERCHANT

    credit_share = float(event.spender.persona.cc_share)

    if request.rng.coin(credit_share):
        return card_acct, CARD_PURCHASE

    return deposit_acct, MERCHANT


def draft_merchant_spec(
    request: GenerateRequest,
    event: Event,
    amount: float,
    dst_acct: str,
) -> TransactionDraft:
    """Builds the draft with a pre-resolved destination account."""
    src_acct, channel = determine_payment_method(request, event)

    return TransactionDraft(
        source=src_acct,
        destination=dst_acct,
        amount=amount,
        timestamp=event.ts,
        channel=channel,
    )
