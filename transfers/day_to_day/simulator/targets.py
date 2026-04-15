from datetime import datetime

from math_models.counts import DEFAULT_RATES, weekday_multiplier

from ..actors import Spender, count_liquidity_factor
from ..environment import Market


def target_txns(market: Market, num_spenders: int) -> float:
    return (
        float(market.merchants_cfg.txns_per_month)
        * float(num_spenders)
        * (float(market.days) / 30.0)
    )


def base_rate_for_target(
    spender: Spender,
    day_shock: float,
    day_start: datetime,
    target_realized_per_day: float,
    dynamics_multiplier: float,
    liquidity_multiplier_value: float,
) -> float:
    if target_realized_per_day <= 0.0:
        return 0.0

    suppression = (
        float(spender.persona.rate_multiplier)
        * float(weekday_multiplier(day_start, DEFAULT_RATES))
        * float(day_shock)
        * max(0.0, float(dynamics_multiplier))
        * count_liquidity_factor(liquidity_multiplier_value)
    )

    if suppression <= 0.0:
        return 0.0

    return target_realized_per_day / suppression
