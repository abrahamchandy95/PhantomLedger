from dataclasses import dataclass

from common.probability import lognormal_by_median
from common.rng import Rng


@dataclass(frozen=True, slots=True)
class AmountModels:
    """
    Centralized configuration for amount distributions.

    Defaults mirror your original implementation.
    """

    salary_median: float = 3000.0
    salary_sigma: float = 0.35
    salary_floor: float = 50.0

    bill_shape: float = 2.0
    bill_scale: float = 400.0
    bill_add: float = 50.0
    bill_floor: float = 1.0

    p2p_median: float = 45.0
    p2p_sigma: float = 0.8
    p2p_floor: float = 1.0

    fraud_median: float = 900.0
    fraud_sigma: float = 0.7
    fraud_floor: float = 50.0

    cycle_median: float = 600.0
    cycle_sigma: float = 0.25
    cycle_floor: float = 1.0

    boost_cycle_median: float = 500.0
    boost_cycle_sigma: float = 0.2
    boost_cycle_floor: float = 1.0


DEFAULT_AMOUNT_MODELS = AmountModels()


def _lognormal_amount(rng: Rng, *, median: float, sigma: float, floor: float) -> float:
    amt = float(
        lognormal_by_median(
            rng.gen,
            median=median,
            sigma=sigma,
        )
    )
    return round(max(floor, amt), 2)


def _gamma_amount(
    rng: Rng, *, shape: float, scale: float, add: float, floor: float
) -> float:
    amt = float(rng.gen.gamma(shape=shape, scale=scale)) + add
    return round(max(floor, amt), 2)


def salary_amount(rng: Rng, models: AmountModels = DEFAULT_AMOUNT_MODELS) -> float:
    return _lognormal_amount(
        rng,
        median=models.salary_median,
        sigma=models.salary_sigma,
        floor=models.salary_floor,
    )


def bill_amount(rng: Rng, models: AmountModels = DEFAULT_AMOUNT_MODELS) -> float:
    return _gamma_amount(
        rng,
        shape=models.bill_shape,
        scale=models.bill_scale,
        add=models.bill_add,
        floor=models.bill_floor,
    )


def p2p_amount(rng: Rng, models: AmountModels = DEFAULT_AMOUNT_MODELS) -> float:
    return _lognormal_amount(
        rng,
        median=models.p2p_median,
        sigma=models.p2p_sigma,
        floor=models.p2p_floor,
    )


def fraud_amount(rng: Rng, models: AmountModels = DEFAULT_AMOUNT_MODELS) -> float:
    return _lognormal_amount(
        rng,
        median=models.fraud_median,
        sigma=models.fraud_sigma,
        floor=models.fraud_floor,
    )


def cycle_amount(rng: Rng, models: AmountModels = DEFAULT_AMOUNT_MODELS) -> float:
    return _lognormal_amount(
        rng,
        median=models.cycle_median,
        sigma=models.cycle_sigma,
        floor=models.cycle_floor,
    )


def boost_cycle_amount(rng: Rng, models: AmountModels = DEFAULT_AMOUNT_MODELS) -> float:
    return _lognormal_amount(
        rng,
        median=models.boost_cycle_median,
        sigma=models.boost_cycle_sigma,
        floor=models.boost_cycle_floor,
    )
