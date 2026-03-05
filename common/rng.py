import numpy as np
from collections.abc import Sequence
from dataclasses import dataclass
from typing import TypeVar, cast

T = TypeVar("T")


@dataclass(frozen=True, slots=True)
class Rng:
    """
    Small wrapper around NumPy Generator to centralize sampling behavior.

    Keep ALL random sampling helpers here so other modules don't duplicate logic.
    """

    gen: np.random.Generator

    @classmethod
    def from_seed(cls, seed: int) -> "Rng":
        return cls(np.random.default_rng(seed))

    def coin(self, p: float) -> bool:
        """Bernoulli(p)."""
        if p <= 0.0:
            return False
        if p >= 1.0:
            return True
        return float(self.gen.random()) < p

    def int(self, low: int, high: int) -> int:
        """Uniform integer in [low, high) (matches np.integers)."""
        return int(self.gen.integers(low, high))

    def float(self) -> float:
        """Uniform float in [0, 1)."""
        return float(self.gen.random())

    def choice(self, items: Sequence[T]) -> T:
        """Uniform choice from a non-empty sequence."""
        if len(items) == 0:
            raise ValueError("choice() requires a non-empty sequence")
        idx = int(self.gen.integers(0, len(items)))
        return items[idx]

    def choice_k(self, items: Sequence[T], k: int, *, replace: bool = False) -> list[T]:
        """
        Choose k items (optionally without replacement).
        Returns a Python list.
        """
        if k < 0:
            raise ValueError("k must be >= 0")
        if k == 0:
            return []
        if len(items) == 0:
            raise ValueError("choice_k() requires a non-empty sequence when k > 0")
        if not replace and k > len(items):
            raise ValueError("k cannot exceed len(items) when replace=False")

        idxs_raw = self.gen.choice(len(items), size=k, replace=replace)
        idxs_arr = np.asarray(idxs_raw, dtype=np.int64)

        # NumPy's tolist() type is loose; force it to list[int] for strict checkers.
        idxs_list = cast(list[int], idxs_arr.tolist())

        return [items[i] for i in idxs_list]

    def weighted_choice(self, items: Sequence[T], weights: Sequence[float]) -> T:
        """
        Choose 1 item from items with probability proportional to weights.
        """
        if len(items) == 0:
            raise ValueError("weighted_choice() requires a non-empty sequence")
        if len(items) != len(weights):
            raise ValueError("items and weights must have the same length")

        p = normalize_weights(weights)
        idx = int(self.gen.choice(len(items), p=p))
        return items[idx]


def normalize_weights(weights: Sequence[float]) -> np.ndarray:
    """
    Normalize non-negative weights into a probability vector (float64 ndarray).

    Uses Python's sum() to avoid Any leakage from some NumPy stub paths.
    This is called rarely (e.g., once per weight vector), so it's fine.
    """
    w = np.asarray(weights, dtype=np.float64)

    if w.ndim != 1:
        raise ValueError("weights must be 1-dimensional")
    if w.size == 0:
        raise ValueError("weights must be non-empty")
    if np.any(w < 0.0):
        raise ValueError("weights must be non-negative")

    s = float(sum(weights))
    if not np.isfinite(s) or s <= 0.0:
        raise ValueError("weights must sum to a finite positive value")

    return w / s
