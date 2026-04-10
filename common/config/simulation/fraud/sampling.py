"""
Stochastic ring-parameter sampling for the Fraud configuration.
"""

from dataclasses import dataclass
from math import log

from common.random import Rng
from .rates import Fraud


@dataclass(frozen=True, slots=True)
class RingParams:
    """Per-ring parameters drawn from Fraud distributions."""

    ring_size: int
    mule_count: int
    victim_count: int


@dataclass(frozen=True, slots=True)
class FraudSampler:
    cfg: Fraud
    rng: Rng

    def ring_count(self, population: int) -> int:
        mean_rate = float(self.cfg.rings_per_10k_mean)
        sigma = float(self.cfg.rings_per_10k_sigma)

        if mean_rate <= 0.0:
            return 0

        if sigma > 0.0:
            mu = log(mean_rate) - (sigma**2) / 2.0
            rate = float(self.rng.gen.lognormal(mean=mu, sigma=sigma))
        else:
            rate = mean_rate

        return max(0, int(round(rate * (population / 10_000.0))))

    def ring_params(
        self, remaining_budget: int, eligible_victims: int
    ) -> RingParams | None:
        if remaining_budget < int(self.cfg.ring_size_min):
            return None

        # --- ring size ---
        raw_size = float(
            self.rng.gen.lognormal(
                mean=float(self.cfg.ring_size_mu),
                sigma=float(self.cfg.ring_size_sigma),
            )
        )
        ring_size = int(round(raw_size))
        ring_size = max(
            int(self.cfg.ring_size_min), min(int(self.cfg.ring_size_max), ring_size)
        )
        ring_size = min(ring_size, remaining_budget)

        if ring_size < int(self.cfg.ring_size_min):
            return None

        # --- mule fraction ---
        mule_frac = float(
            self.rng.gen.beta(
                float(self.cfg.mule_frac_alpha),
                float(self.cfg.mule_frac_beta),
            )
        )
        mule_frac = max(
            float(self.cfg.mule_frac_min),
            min(float(self.cfg.mule_frac_max), mule_frac),
        )
        mule_count = max(1, int(round(mule_frac * ring_size)))
        mule_count = min(mule_count, ring_size - 1)

        # --- victim count ---
        raw_victims = float(
            self.rng.gen.lognormal(
                mean=float(self.cfg.victims_mu),
                sigma=float(self.cfg.victims_sigma),
            )
        )
        victim_count = int(round(raw_victims))
        victim_count = max(
            int(self.cfg.victims_min), min(int(self.cfg.victims_max), victim_count)
        )
        victim_count = min(victim_count, max(0, eligible_victims))

        return RingParams(
            ring_size=ring_size,
            mule_count=mule_count,
            victim_count=victim_count,
        )

    def solo_count(self, population: int, remaining_budget: int) -> int:
        rate = float(self.cfg.solos_per_10k) * (population / 10_000.0)
        if rate <= 0.0:
            return 0

        count = int(self.rng.gen.poisson(lam=rate))
        return min(count, max(0, remaining_budget))
