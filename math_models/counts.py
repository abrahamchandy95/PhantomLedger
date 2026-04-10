from dataclasses import dataclass
from datetime import datetime

from common.random import Rng


@dataclass(frozen=True, slots=True)
class Rates:
    """
    Statistical parameters that control transaction count generation.

    We use a Gamma-Poisson mixture to create overdispersed counts:
      lambda_day ~ Gamma(shape=k, scale=base_rate/k)
      n_out      ~ Poisson(lambda_day)

    This behaves like a Negative Binomial (bursty / heavy-tailed counts).
    """

    gamma_shape_k: float = 1.5
    weekend_multiplier: float = 0.8

    max_txns_per_account_per_day: int = 0


DEFAULT_RATES = Rates()


def weekday_multiplier(day_start: datetime, rates: Rates = DEFAULT_RATES) -> float:
    """
    Returns a multiplier for activity based on day-of-week.
    Weekend (Sat/Sun) gets reduced activity.
    """
    dow: int = day_start.weekday()  # Mon=0 .. Sun=6
    return rates.weekend_multiplier if dow >= 5 else 1.0


def gamma_poisson(
    rng: Rng,
    *,
    base_rate: float,
    rates: Rates = DEFAULT_RATES,
) -> int:
    """
    Generate an overdispersed transaction count for one account-day.

    base_rate: the "typical" mean for that account-day before overdispersion.
    """
    if base_rate <= 0.0:
        return 0

    k = rates.gamma_shape_k
    if k <= 0.0:
        raise ValueError("gamma_shape_k must be > 0")

    # Gamma-Poisson mixture: lambda ~ Gamma(k, scale=base_rate/k)
    lam_day = float(rng.gen.gamma(shape=k, scale=max(1e-12, base_rate / k)))

    n_out = int(rng.gen.poisson(lam=lam_day))

    cap = rates.max_txns_per_account_per_day
    if cap > 0 and n_out > cap:
        n_out = cap

    return n_out
