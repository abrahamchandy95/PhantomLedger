from dataclasses import dataclass

from common.validate import (
    require_float_between,
    require_int_ge,
)


@dataclass(frozen=True, slots=True)
class FraudConfig:
    num_rings: int = 46
    ring_size: int = 12
    mules_per_ring: int = 4
    victims_per_ring: int = 60

    target_illicit_ratio: float = 0.00009

    def fraudsters_per_ring(self) -> int:
        ring_size = int(self.ring_size)
        mules = int(self.mules_per_ring)
        return max(1, ring_size - mules)

    def expected_fraudsters(self) -> int:
        return int(self.num_rings) * self.fraudsters_per_ring()

    def expected_mules(self) -> int:
        rings = int(self.num_rings)
        ring_size = int(self.ring_size)
        mules = int(self.mules_per_ring)
        return rings * min(mules, max(0, ring_size - 1))

    def validate(self, *, persons: int) -> None:
        require_int_ge("num_rings", self.num_rings, 0)
        require_float_between(
            "target_illicit_ratio",
            self.target_illicit_ratio,
            0.0,
            0.5,
        )

        if self.num_rings == 0:
            return

        require_int_ge("ring_size", self.ring_size, 3)
        require_int_ge("mules_per_ring", self.mules_per_ring, 0)
        require_int_ge("victims_per_ring", self.victims_per_ring, 0)

        ring_people = int(self.num_rings) * int(self.ring_size)
        if ring_people >= int(persons):
            raise ValueError(
                "fraud ring participants exceed/consume the population size"
            )

        eligible = int(persons) - ring_people
        if int(self.victims_per_ring) > eligible:
            raise ValueError(
                "victims_per_ring too large for eligible non-ring population"
            )
