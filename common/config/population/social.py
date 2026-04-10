from dataclasses import dataclass

from common.validate import between, gt


@dataclass(frozen=True, slots=True)
class Social:
    target_degree: int = 12
    local_prob: float = 0.70
    hub_multiplier: float = 25.0
    influence_sigma: float = 1.1
    tie_strength_shape: float = 1.0

    def __post_init__(self) -> None:
        """Automatically called by the dataclass after initialization."""
        gt("target_degree", self.target_degree, 0)
        between("local_prob", self.local_prob, 0.0, 1.0)
        gt("hub_multiplier", self.hub_multiplier, 0.0)
        gt("influence_sigma", self.influence_sigma, 0.0)
        gt("tie_strength_shape", self.tie_strength_shape, 0.0)

    @property
    def effective_degree(self) -> int:
        return max(3, min(24, int(self.target_degree)))

    @property
    def cross_prob(self) -> float:
        return max(0.01, min(0.25, 1.0 - float(self.local_prob)))

    def community_bounds(self) -> tuple[int, int]:
        k = self.effective_degree
        c_min = max(20, 6 * k)
        c_max = max(c_min + 20, 24 * k)
        return c_min, c_max
