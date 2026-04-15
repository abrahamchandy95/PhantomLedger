from dataclasses import dataclass

from common.random import Rng
from common.transactions import Transaction

from ..actors import Event
from ..environment import Market
from ..payments import bill, external, merchant, p2p


@dataclass(frozen=True, slots=True)
class RoutePolicy:
    prefer_billers_p: float
    max_retries: int


def route_txn(
    rng: Rng,
    market: Market,
    policy: RoutePolicy,
    channel_idx: int,
    event: Event,
) -> Transaction | None:
    if channel_idx == 2:
        return p2p(rng, market.population, market.social, event)

    if channel_idx == 1:
        return bill(rng, market.merchants, event, policy.prefer_billers_p)

    if channel_idx == 3:
        return external(rng, market.merchants.merchants, event)

    return merchant(
        rng,
        market.merchants,
        market.credit_cards,
        event,
        policy.max_retries,
    )
