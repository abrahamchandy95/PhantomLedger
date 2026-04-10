from collections.abc import Sized
from typing import TypeVar

N = TypeVar("N", int, float)
S = TypeVar("S", bound=Sized)


def gt(name: str, v: N, lo: N) -> None:
    """Guard: value must be strictly greater than lo."""
    if v <= lo:
        raise ValueError(f"'{name}' must be > {lo}, got {v}")


def ge(name: str, v: N, lo: N) -> None:
    """Guard: value must be greater than or equal to lo."""
    if v < lo:
        raise ValueError(f"'{name}' must be >= {lo}, got {v}")


def between(name: str, v: N, lo: N, hi: N) -> None:
    """Guard: value must be within the inclusive range [lo, hi]."""
    if not (lo <= v <= hi):
        raise ValueError(f"'{name}' must be in range [{lo}, {hi}], got {v}")


def nonempty(name: str, v: Sized) -> None:
    """Guard: container or string must have a length > 0."""
    if len(v) == 0:
        raise ValueError(f"'{name}' cannot be empty")
