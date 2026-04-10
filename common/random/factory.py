import hashlib
from dataclasses import dataclass

from .rng import Rng


def derive_seed(base: int, *parts: str) -> int:
    """
    Deterministically derive a 64-bit seed from a base and string parts.
    """
    key = f"{base}|{'|'.join(parts)}".encode("utf-8")

    h = hashlib.blake2b(key, digest_size=8)
    return int.from_bytes(h.digest(), "little", signed=False)


@dataclass(frozen=True, slots=True)
class RngFactory:
    """
    Centralized factory for vending deterministic Rng instances.
    """

    base: int

    def rng(self, *parts: str) -> Rng:
        """Vend a strictly-typed Rng wrapper for a specific namespace."""
        seed = derive_seed(self.base, *parts)
        return Rng.from_seed(seed)
