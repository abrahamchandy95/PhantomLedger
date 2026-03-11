from typing import cast

import numpy as np

from common.math import ArrI32, as_int, cdf_pick
from transfers.txns import TxnSpec

from .models import DayToDayGenerationRequest
from .state import EventFrame


def row_contains(row: ArrI32, k: int, value: int) -> bool:
    for idx in range(k):
        if as_int(cast(int | np.integer, row[idx])) == value:
            return True
    return False


def pick_merchant_destination(
    request: DayToDayGenerationRequest,
    event_frame: EventFrame,
) -> str:
    ctx = request.ctx
    person_idx = event_frame.person_idx
    person_state = event_frame.person_state

    exploring = request.rng.coin(event_frame.explore_p)

    if (not exploring) and person_state.favorite_k > 0:
        merchant_idx = as_int(
            cast(
                int | np.integer,
                ctx.fav_merchants_idx[
                    person_idx,
                    request.rng.int(0, person_state.favorite_k),
                ],
            )
        )
    else:
        merchant_idx = cdf_pick(ctx.merch_cdf, request.rng.float())
        favorite_row: ArrI32 = np.asarray(
            ctx.fav_merchants_idx[person_idx, :],
            dtype=np.int32,
        )

        tries = 0
        retry_limit = int(request.policy.merchant_retry_limit)
        while (
            person_state.favorite_k > 0
            and row_contains(favorite_row, person_state.favorite_k, merchant_idx)
            and tries < retry_limit
        ):
            merchant_idx = cdf_pick(ctx.merch_cdf, request.rng.float())
            tries += 1

    return ctx.merchants.counterparty_accts[merchant_idx]


def pick_merchant_source(
    request: DayToDayGenerationRequest,
    event_frame: EventFrame,
) -> tuple[str, str]:
    src_acct = event_frame.person_state.deposit_acct
    channel = "merchant"

    policy = request.credit_policy
    card_map = request.card_for_person
    if policy is None or card_map is None or not card_map:
        return src_acct, channel

    card_acct = card_map.get(event_frame.person_state.person_id)
    if card_acct is None:
        return src_acct, channel

    credit_share = float(
        policy.merchant_credit_share(event_frame.person_state.persona_name)
    )
    if request.rng.coin(credit_share):
        return card_acct, "card_purchase"

    return src_acct, channel


def build_merchant_txn_spec(
    request: DayToDayGenerationRequest,
    event_frame: EventFrame,
    amount: float,
) -> TxnSpec:
    dst_acct = pick_merchant_destination(request, event_frame)
    src_acct, channel = pick_merchant_source(request, event_frame)

    return TxnSpec(
        src=src_acct,
        dst=dst_acct,
        amt=amount,
        ts=event_frame.txn_ts,
        channel=channel,
    )
