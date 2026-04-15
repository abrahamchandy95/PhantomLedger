from typing import cast

import numpy as np

from common.channels import BILL, EXTERNAL_UNKNOWN, P2P
from common.math import as_int
from common.random import Rng
from common.transactions import Transaction
from entities import models as entity_models
from math_models.amount_model import (
    BILL as BILL_MODEL,
    EXTERNAL_UNKNOWN as EXTERNAL_MODEL,
    P2P as P2P_MODEL,
    merchant_amount,
)
from relationships.social import Graph
from transfers.factory import TransactionDraft

from .environment import MerchantView, PopulationView
from .merchant import choose_merchant, draft_merchant_spec
from .actors import Event


def p2p(
    rng: Rng, pop: PopulationView, social: Graph, event: Event
) -> Transaction | None:
    spender = event.spender

    contact_idx = as_int(
        cast(
            int | np.integer,
            social.contacts[event.person_idx, rng.int(0, social.degree)],
        )
    )

    if not (0 <= contact_idx < len(pop.persons)):
        return None

    dst_person = pop.persons[contact_idx]
    dst_acct = pop.primary_accounts.get(dst_person)

    if not dst_acct or dst_acct == spender.deposit_acct:
        return None

    amount = P2P_MODEL.sample(rng) * float(spender.persona.amount_multiplier)
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
    rng: Rng, merch: MerchantView, event: Event, prefer_billers_p: float
) -> Transaction:
    spender = event.spender

    if spender.bill_k > 0 and rng.coin(prefer_billers_p):
        idx = rng.int(0, max(1, spender.bill_k))
        biller_idx = as_int(
            cast(int | np.integer, merch.billers_idx[event.person_idx, idx])
        )
    else:
        biller_idx = as_int(
            cast(
                int | np.integer,
                np.searchsorted(merch.biller_cdf, rng.float(), side="right"),
            )
        )
        if biller_idx >= int(merch.biller_cdf.size):
            biller_idx = int(merch.biller_cdf.size) - 1

    dst_acct = merch.merchants.counterparties[biller_idx]
    amount = BILL_MODEL.sample(rng)

    return event.txf.make(
        TransactionDraft(
            source=spender.deposit_acct,
            destination=dst_acct,
            amount=amount,
            timestamp=event.ts,
            channel=BILL,
        )
    )


def external(rng: Rng, merchants: entity_models.Merchants, event: Event) -> Transaction:
    """
    External unknown outflow.


    Uses EXTERNAL_UNKNOWN model (median $120, sigma 0.95)
    per Fed Payments Study 2024 non-card remote payment data.
    """
    spender = event.spender

    if merchants.externals:
        dst_acct = rng.choice(merchants.externals)
    else:
        dst_acct = "X0000000001"

    amount = EXTERNAL_MODEL.sample(rng)
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


def merchant(
    rng: Rng,
    merch: MerchantView,
    cards: dict[str, str] | None,
    event: Event,
    max_retries: int,
) -> Transaction:
    spender = event.spender

    merch_idx = choose_merchant(rng, merch, event, max_retries)
    dst_acct = merch.merchants.counterparties[merch_idx]
    category = merch.merchants.categories[merch_idx]

    amount = merchant_amount(rng, category)
    amount *= float(spender.persona.amount_multiplier)
    amount = round(max(1.0, amount), 2)

    spec = draft_merchant_spec(rng, cards, event, amount, dst_acct)
    return event.txf.make(spec)
