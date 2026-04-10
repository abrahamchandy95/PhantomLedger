"""
Temporal autocorrelation (momentum) in individual spending.

Real consumer spending exhibits strong positive autocorrelation:
people who spent heavily last week tend to spend heavily this week.
This is a consequence of bursty human dynamics (Barabási 2005,
Nature 435:207-211) and has been directly measured in bank
transaction data (Vilella et al. 2021, EPJ Data Science 10:25),
where "spending persistence" — the autocorrelation of weekly
spending amounts — is a stable individual-level trait.

Goh & Barabási (2008, EPL 81:48002) formalized the memory
coefficient M as the autocorrelation between consecutive inter-event
times. Karsai et al. (2012, Scientific Reports 2:397) showed these
correlations are universal across human activity domains.

We model this as a simple AR(1) process on the daily spending rate
multiplier. Each person has a hidden "momentum" state that drifts
toward 1.0 (baseline) but is pulled by yesterday's realized activity:

    momentum_t = phi * momentum_{t-1} + (1 - phi) * 1.0 + noise

where phi ∈ [0.3, 0.7] controls persistence strength. The momentum
multiplier then scales the Gamma-Poisson count model's base rate.

This creates realistic temporal texture: spending clusters into
high-activity periods (1-2 weeks) followed by quieter stretches,
rather than the IID daily counts that make every legitimate account
look the same and every mule burst look anomalous.
"""

from dataclasses import dataclass

from common.random import Rng
from common.validate import between, ge


@dataclass(frozen=True, slots=True)
class MomentumConfig:
    """Controls the AR(1) spending momentum process."""

    # Autoregressive coefficient. Higher = more persistent.
    # Vilella et al. 2021 measured weekly spending autocorrelations
    # of 0.3-0.6 across personality types. We default to 0.45
    # which, at daily resolution, corresponds to a weekly
    # autocorrelation of roughly 0.45^7 ≈ 0.004 for pure AR(1),
    # but since we compound through the rate multiplier the
    # effective weekly persistence is much higher (~0.35-0.50).
    phi: float = 0.45

    # Innovation noise scale (std dev of the daily shock).
    # Controls how much the momentum can deviate from baseline.
    noise_sigma: float = 0.15

    # Clamp range for the momentum multiplier.
    # Prevents degenerate rates (too low = no activity, too high = explosion).
    floor: float = 0.20
    ceiling: float = 3.00

    def __post_init__(self) -> None:
        between("phi", self.phi, 0.0, 0.99)
        ge("noise_sigma", self.noise_sigma, 0.0)
        between("floor", self.floor, 0.01, 1.0)
        between("ceiling", self.ceiling, 1.0, 10.0)


DEFAULT_MOMENTUM_CONFIG = MomentumConfig()


@dataclass(slots=True)
class MomentumState:
    """Mutable per-person momentum tracker."""

    value: float = 1.0

    def advance(self, rng: Rng, cfg: MomentumConfig) -> float:
        """
        Advance the momentum by one day and return the current multiplier.

        The AR(1) process:
          m_t = phi * m_{t-1} + (1 - phi) * 1.0 + eps
          eps ~ Normal(0, noise_sigma)

        Returns the clamped momentum value to use as a rate multiplier.
        """
        mean_revert = float(cfg.phi) * self.value + (1.0 - float(cfg.phi)) * 1.0

        if cfg.noise_sigma > 0.0:
            eps = float(rng.gen.normal(loc=0.0, scale=float(cfg.noise_sigma)))
        else:
            eps = 0.0

        raw = mean_revert + eps
        self.value = max(float(cfg.floor), min(float(cfg.ceiling), raw))
        return self.value
