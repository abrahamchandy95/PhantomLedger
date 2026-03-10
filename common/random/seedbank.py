from dataclasses import dataclass

import numpy as np

from .seeding import derived_seed


@dataclass(frozen=True, slots=True)
class SeedBank:
    base_seed: int

    def generator(self, *parts: str) -> np.random.Generator:
        return np.random.default_rng(derived_seed(self.base_seed, *parts))
