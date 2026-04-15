from typing import cast

from common.channels import CARD_PURCHASE, MERCHANT
from common.math import cdf_pick
from common.random import Rng
from transfers.factory import TransactionDraft

from .environment import MerchantView
from .actors import Event


def choose_merchant(
    rng: Rng, merch: MerchantView, event: Event, max_reties: int
) -> int:
    """Returns the merchant index so callers can look up category."""
    spender = event.spender
    idx = event.person_idx

    exploring = rng.coin(event.explore_p)

    if not exploring and spender.fav_k > 0:
        fav_idx = rng.int(0, spender.fav_k)
        merchant_idx = int(cast(int, merch.fav_merchants_idx[idx, fav_idx]))

    else:
        merchant_idx = cdf_pick(merch.merch_cdf, rng.float())

        if spender.fav_k > 0:
            favorites = set(
                cast(list[int], merch.fav_merchants_idx[idx, : spender.fav_k].tolist())
            )
            tries = 0

            while merchant_idx in favorites and tries < max_reties:
                merchant_idx = cdf_pick(merch.merch_cdf, rng.float())
                tries += 1

    return merchant_idx


def determine_payment_method(
    rng: Rng,
    cards: dict[str, str] | None,
    event: Event,
) -> tuple[str, str]:
    deposit_acct = event.spender.deposit_acct

    if not cards:
        return deposit_acct, MERCHANT

    card_acct = cards.get(event.spender.id)
    if not card_acct:
        return deposit_acct, MERCHANT

    credit_share = float(event.spender.persona.cc_share)

    if rng.coin(credit_share):
        return card_acct, CARD_PURCHASE

    return deposit_acct, MERCHANT


def draft_merchant_spec(
    rng: Rng,
    cards: dict[str, str] | None,
    event: Event,
    amount: float,
    dst_acct: str,
) -> TransactionDraft:
    """Builds the draft with a pre-resolved destination account."""
    src_acct, channel = determine_payment_method(rng, cards, event)

    return TransactionDraft(
        source=src_acct,
        destination=dst_acct,
        amount=amount,
        timestamp=event.ts,
        channel=channel,
    )
