from collections.abc import Iterator
from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True, slots=True)
class IdFormat:
    prefix: str
    width: int

    def format(self, seq_num: int) -> str:
        if seq_num <= 0:
            raise ValueError(f"ID sequence number must be >= 1, got {seq_num}")
        return f"{self.prefix}{seq_num:0{self.width}d}"


PERSON_ID = IdFormat(prefix="C", width=7)
ACCOUNT_ID = IdFormat(prefix="A", width=9)
MERCHANT_ACCOUNT_ID = IdFormat(prefix="M", width=9)
EXTERNAL_ACCOUNT_ID = IdFormat(prefix="X", width=10)


def person_id(seq_num: int) -> str:
    """1-based person id."""
    return PERSON_ID.format(seq_num)


def account_id(seq_num: int) -> str:
    """1-based account id."""
    return ACCOUNT_ID.format(seq_num)


def iter_person_ids(count: int) -> Iterator[str]:
    """Generate Cxxxxxxx ids (1..count)."""
    if count < 0:
        raise ValueError("count must be >= 0")
    for i in range(1, count + 1):
        yield person_id(i)


def iter_account_ids(count: int) -> Iterator[str]:
    """Generate Axxxxxxxxx ids (1..count)."""
    if count < 0:
        raise ValueError("count must be >= 0")
    for i in range(1, count + 1):
        yield account_id(i)


def device_id_for_person(person_id_str: str, index: int) -> str:
    """
    Deterministic per-person device id.
    Mirrors the original shape: D{person[1:]}_{j+1}
    Example: person_id_str="C0000123", index=1 -> "D0000123_1"
    """
    if index <= 0:
        raise ValueError("index must be >= 1")
    if not person_id_str.startswith("C") or len(person_id_str) < 2:
        raise ValueError("unexpected person_id format")

    return f"D{person_id_str[1:]}_{index}"


def shared_ring_device_id(ring_id: int) -> str:
    """Shared fraud ring device: FD0000, FD0001, ..."""
    if ring_id < 0:
        raise ValueError("ring_id must be >= 0")
    return f"FD{ring_id:04d}"


def rand_ipv4(rng: np.random.Generator) -> str:
    """
    Generate a 'public-looking' IPv4 address.
    Not a perfect filter of all reserved ranges, but avoids very low/high first octets.
    """
    # Optimized: Generate all 4 octets at once using arrays of bounds.
    # Note: rng.integers high bound is exclusive.
    octets = rng.integers([11, 0, 0, 1], [223, 256, 256, 255])
    return f"{octets[0]}.{octets[1]}.{octets[2]}.{octets[3]}"


def merchant_account_id(seq_num: int) -> str:
    """1-based merchant account id."""
    return MERCHANT_ACCOUNT_ID.format(seq_num)


def external_account_id(seq_num: int) -> str:
    """1-based external counterparty id."""
    return EXTERNAL_ACCOUNT_ID.format(seq_num)


def is_external_account(acct_id: str) -> bool:
    return acct_id.startswith("X")
