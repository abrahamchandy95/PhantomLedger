from typing import cast

import numpy as np

from common.math import as_int
from common.transactions import Transaction
from math_models.amounts import bill_amount, p2p_amount
from transfers.txns import TxnSpec

from .merchant import build_merchant_txn_spec
from .models import DayToDayGenerationRequest
from .state import EventFrame


def emit_p2p_txn(
    request: DayToDayGenerationRequest,
    event_frame: EventFrame,
) -> Transaction | None:
    ctx = request.ctx

    contact_idx = as_int(
        cast(
            int | np.integer,
            ctx.social.contacts[
                event_frame.person_idx,
                request.rng.int(0, ctx.social.k_contacts),
            ],
        )
    )
    if not (0 <= contact_idx < len(ctx.persons)):
        return None

    dst_person = ctx.persons[contact_idx]
    dst_acct = ctx.primary_acct_for_person.get(dst_person)
    if dst_acct is None or dst_acct == event_frame.person_state.deposit_acct:
        return None

    amount = float(p2p_amount(request.rng)) * float(
        event_frame.person_state.persona.amount_mult
    )
    amount = round(max(1.0, amount), 2)

    return event_frame.txf.make(
        TxnSpec(
            src=event_frame.person_state.deposit_acct,
            dst=dst_acct,
            amt=amount,
            ts=event_frame.txn_ts,
            channel="p2p",
        )
    )


def emit_bill_txn(
    request: DayToDayGenerationRequest,
    event_frame: EventFrame,
    prefer_billers_p: float,
) -> Transaction:
    ctx = request.ctx
    person_state = event_frame.person_state

    if person_state.biller_k > 0 and request.rng.coin(prefer_billers_p):
        biller_idx = as_int(
            cast(
                int | np.integer,
                ctx.billers_idx[
                    event_frame.person_idx,
                    request.rng.int(0, max(1, person_state.biller_k)),
                ],
            )
        )
    else:
        biller_idx = as_int(
            cast(
                int | np.integer,
                np.searchsorted(
                    ctx.global_biller_cdf,
                    request.rng.float(),
                    side="right",
                ),
            )
        )
        if biller_idx >= int(ctx.global_biller_cdf.size):
            biller_idx = int(ctx.global_biller_cdf.size) - 1

    dst_acct = ctx.merchants.counterparty_accts[biller_idx]
    amount = float(bill_amount(request.rng))

    return event_frame.txf.make(
        TxnSpec(
            src=person_state.deposit_acct,
            dst=dst_acct,
            amt=amount,
            ts=event_frame.txn_ts,
            channel="bill",
        )
    )


def emit_external_unknown_txn(
    request: DayToDayGenerationRequest,
    event_frame: EventFrame,
) -> Transaction:
    merchants = request.ctx.merchants

    if merchants.external_accounts:
        dst_acct = request.rng.choice(merchants.external_accounts)
    else:
        dst_acct = "X0000000001"

    amount = float(p2p_amount(request.rng))
    amount = round(max(1.0, amount), 2)

    return event_frame.txf.make(
        TxnSpec(
            src=event_frame.person_state.deposit_acct,
            dst=dst_acct,
            amt=amount,
            ts=event_frame.txn_ts,
            channel="external_unknown",
        )
    )


def emit_merchant_txn(
    request: DayToDayGenerationRequest,
    event_frame: EventFrame,
) -> Transaction:
    amount = float(p2p_amount(request.rng)) * float(
        event_frame.person_state.persona.amount_mult
    )
    amount = round(max(1.0, amount), 2)

    spec = build_merchant_txn_spec(request, event_frame, amount)
    return event_frame.txf.make(spec)
