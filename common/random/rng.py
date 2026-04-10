from collections.abc import Sequence
from dataclasses import dataclass
from typing import Self, TypeVar, cast

import numpy as np

from common.math import build_cdf, cdf_pick
from common.validate import ge

T = TypeVar("T")


@dataclass(frozen=True, slots=True)
class Rng:
    """
    Wrapper around NumPy Generator to centralize sampling behavior.
    """

    gen: np.random.Generator

    @classmethod
    def from_seed(cls, seed: int) -> Self:
        return cls(np.random.default_rng(seed))

    def coin(self, p: float) -> bool:
        """Bernoulli trial: True with probability p."""
        if p <= 0.0:
            return False
        if p >= 1.0:
            return True
        return float(self.gen.random()) < p

    def int(self, low: int, high: int) -> int:
        """Uniform integer in [low, high)."""
        return int(self.gen.integers(low, high))

    def float(self) -> float:
        """Uniform float in [0, 1)."""
        return float(self.gen.random())

    def choice(self, items: Sequence[T]) -> T:
        """Uniformly pick one item from a non-empty sequence."""
        if not items:
            raise ValueError("choice() requires a non-empty sequence")
        return items[self.int(0, len(items))]

    def choice_k(self, items: Sequence[T], k: int, *, replace: bool = False) -> list[T]:
        """Pick k items from a sequence."""
        ge("k", k, 0)
        if k == 0:
            return []

        n = len(items)
        if n == 0:
            raise ValueError("choice_k() requires a non-empty sequence when k > 0")
        if not replace and k > n:
            raise ValueError(
                f"k ({k}) cannot exceed item count ({n}) without replacement"
            )

        idxs = self.gen.choice(n, size=k, replace=replace)
        idxs = cast(list[int], idxs.tolist())
        return [items[int(i)] for i in idxs]

    def weighted_choice(self, items: Sequence[T], weights: Sequence[float]) -> T:
        """Pick one item based on relative weights."""
        if not items:
            raise ValueError("weighted_choice() requires a non-empty sequence")
        if len(items) != len(weights):
            raise ValueError(
                f"Length mismatch: {len(items)} items vs {len(weights)} weights"
            )

        cdf = build_cdf(weights)
        idx = cdf_pick(cdf, self.float())
        return items[idx]
