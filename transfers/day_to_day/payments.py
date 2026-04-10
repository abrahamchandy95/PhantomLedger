from typing import cast

import numpy as np

from common.channels import BILL, EXTERNAL_UNKNOWN, P2P
from common.math import as_int
from common.transactions import Transaction
from math_models.amount_model import (
    BILL as BILL_MODEL,
    EXTERNAL_UNKNOWN as EXTERNAL_MODEL,
    P2P as P2P_MODEL,
    merchant_amount,
)
from transfers.factory import TransactionDraft

from .engine import GenerateRequest
from .merchant import choose_merchant, draft_merchant_spec
from .state import Event


def p2p(request: GenerateRequest, event: Event) -> Transaction | None:
    ctx = request.ctx
    pop = ctx.population
    spender = event.spender

    contact_idx = as_int(
        cast(
            int | np.integer,
            ctx.social.contacts[
                event.person_idx, request.rng.int(0, ctx.social.degree)
            ],
        )
    )

    if not (0 <= contact_idx < len(pop.persons)):
        return None

    dst_person = pop.persons[contact_idx]
    dst_acct = pop.primary_accounts.get(dst_person)

    if not dst_acct or dst_acct == spender.deposit_acct:
        return None

    amount = P2P_MODEL.sample(request.rng) * float(spender.persona.amount_multiplier)
    amount = round(max(1.0, amount), 2)

    return event.txf.make(
        TransactionDraft(
            source=spender.deposit_acct,
            destination=dst_acct,
            amount=amount,
            timestamp=event.ts,
            channel=P2P,
        )
    )


def bill(
    request: GenerateRequest,
    event: Event,
    prefer_billers_p: float,
) -> Transaction:
    ctx = request.ctx
    merch = ctx.merchant
    spender = event.spender

    if spender.bill_k > 0 and request.rng.coin(prefer_billers_p):
        idx = request.rng.int(0, max(1, spender.bill_k))
        biller_idx = as_int(
            cast(int | np.integer, merch.billers_idx[event.person_idx, idx])
        )
    else:
        biller_idx = as_int(
            cast(
                int | np.integer,
                np.searchsorted(merch.biller_cdf, request.rng.float(), side="right"),
            )
        )
        if biller_idx >= int(merch.biller_cdf.size):
            biller_idx = int(merch.biller_cdf.size) - 1

    dst_acct = merch.merchants.counterparties[biller_idx]
    amount = BILL_MODEL.sample(request.rng)

    return event.txf.make(
        TransactionDraft(
            source=spender.deposit_acct,
            destination=dst_acct,
            amount=amount,
            timestamp=event.ts,
            channel=BILL,
        )
    )


def external(request: GenerateRequest, event: Event) -> Transaction:
    """
    External unknown outflow.

    FIX: Previously used p2p_amount() (median $45, sigma 0.8).
    External unknowns represent wire transfers, online bill pay to
    unrecognized billers, and marketplace purchases — structurally
    different from casual P2P transfers.

    Now uses EXTERNAL_UNKNOWN model (median $120, sigma 0.95)
    per Fed Payments Study 2024 non-card remote payment data.
    """
    merchants = request.ctx.merchant.merchants
    spender = event.spender

    if merchants.externals:
        dst_acct = request.rng.choice(merchants.externals)
    else:
        dst_acct = "X0000000001"

    amount = EXTERNAL_MODEL.sample(request.rng)
    amount = round(max(1.0, amount), 2)

    return event.txf.make(
        TransactionDraft(
            source=spender.deposit_acct,
            destination=dst_acct,
            amount=amount,
            timestamp=event.ts,
            channel=EXTERNAL_UNKNOWN,
        )
    )


def merchant(request: GenerateRequest, event: Event) -> Transaction:
    merch = request.ctx.merchant
    spender = event.spender

    merch_idx = choose_merchant(request, event)
    dst_acct = merch.merchants.counterparties[merch_idx]
    category = merch.merchants.categories[merch_idx]

    amount = merchant_amount(request.rng, category)
    amount *= float(spender.persona.amount_multiplier)
    amount = round(max(1.0, amount), 2)

    spec = draft_merchant_spec(request, event, amount, dst_acct)
    return event.txf.make(spec)
