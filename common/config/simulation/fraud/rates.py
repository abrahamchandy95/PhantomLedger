from dataclasses import dataclass

from common.validate import (
    between,
    ge,
    gt,
)


@dataclass(frozen=True, slots=True)
class Fraud:
    rings_per_10k_mean: float = 6.0
    rings_per_10k_sigma: float = 0.4

    # determined by a log normal distribution
    ring_size_mu: float = 2.0
    ring_size_sigma: float = 0.7
    ring_size_min: int = 3
    ring_size_max: int = 150

    mule_frac_alpha: float = 2.0
    mule_frac_beta: float = 4.0
    mule_frac_min: float = 0.10
    mule_frac_max: float = 0.70

    victims_mu: float = 3.0
    victims_sigma: float = 0.8
    victims_min: int = 3
    victims_max: int = 500

    solos_per_10k: float = 4.0

    multi_ring_mule_p: float = 0.06
    repeat_victim_p: float = 0.10

    max_participation_p: float = 0.06
    target_illicit_p: float = 0.005

    def expected_ring_count(self, population: int) -> int:
        return max(
            0, int(round(float(self.rings_per_10k_mean) * (int(population) / 10_000.0)))
        )

    def max_participants(self, population: int) -> int:
        return int(float(self.max_participation_p) * int(population))

    def expected_solo_count(self, population: int) -> int:
        return max(
            0,
            int(round(float(self.solos_per_10k) * (int(population) / 10_000.0))),
        )

    def __post_init__(self) -> None:
        ge("rings_per_10k_mean", self.rings_per_10k_mean, 0.0)
        ge("rings_per_10k_sigma", self.rings_per_10k_sigma, 0.0)

        gt("ring_size_sigma", self.ring_size_sigma, 0.0)
        ge("ring_size_min", self.ring_size_min, 2)
        ge("ring_size_max", self.ring_size_max, self.ring_size_min)

        gt("mule_frac_alpha", self.mule_frac_alpha, 0.0)
        gt("mule_frac_beta", self.mule_frac_beta, 0.0)
        between("mule_frac_min", self.mule_frac_min, 0.0, 1.0)
        between("mule_frac_max", self.mule_frac_max, 0.0, 1.0)
        if float(self.mule_frac_max) < float(self.mule_frac_min):
            raise ValueError("mule_frac_max must be >= mule_frac_min")

        gt("victims_sigma", self.victims_sigma, 0.0)
        ge("victims_min", self.victims_min, 0)
        ge("victims_max", self.victims_max, self.victims_min)

        ge("solos_per_10k", self.solos_per_10k, 0.0)
        between("multi_ring_mule_p", self.multi_ring_mule_p, 0.0, 1.0)
        between("repeat_victim_p", self.repeat_victim_p, 0.0, 1.0)
        between(
            "max_participation_p",
            self.max_participation_p,
            0.0,
            0.10,
        )
        between(
            "target_illicit_p",
            self.target_illicit_p,
            0.0,
            0.5,
        )
