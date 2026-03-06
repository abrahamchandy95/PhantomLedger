from dataclasses import dataclass
from collections.abc import Iterator
import numpy as np


@dataclass(frozen=True, slots=True)
class IdFormat:
    prefix: str
    width: int

    def format(self, n: int) -> str:
        return f"{self.prefix}{n:0{self.width}d}"


PERSON_ID = IdFormat(prefix="C", width=7)
ACCOUNT_ID = IdFormat(prefix="A", width=9)
MERCHANT_ACCOUNT_ID = IdFormat(prefix="M", width=9)
EXTERNAL_ACCOUNT_ID = IdFormat(prefix="X", width=10)


def person_id(n: int) -> str:
    """1-based person/customer id."""
    if n <= 0:
        raise ValueError("person_id expects n >= 1")
    return PERSON_ID.format(n)


def account_id(n: int) -> str:
    """1-based account id."""
    if n <= 0:
        raise ValueError("account_id expects n >= 1")
    return ACCOUNT_ID.format(n)


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


def device_id_for_person(customer_id: str, index: int) -> str:
    """
    Deterministic per-person device id.
    Mirrors the original shape: D{person[1:]}_{j+1}
    Example: customer_id="C0000123", index=1 -> "D0000123_1"
    """
    if index <= 0:
        raise ValueError("index must be >= 1")
    if not customer_id.startswith("C") or len(customer_id) < 2:
        raise ValueError("unexpected customer_id format")
    return f"D{customer_id[1:]}_{index}"


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
    a = int(rng.integers(11, 223))
    b = int(rng.integers(0, 256))
    c = int(rng.integers(0, 256))
    d = int(rng.integers(1, 255))
    return f"{a}.{b}.{c}.{d}"


def merchant_account_id(n: int) -> str:
    """1-based merchant account id."""
    if n <= 0:
        raise ValueError("merchant_account_id expects n >= 1")
    return MERCHANT_ACCOUNT_ID.format(n)


def external_account_id(n: int) -> str:
    """1-based external counterparty id."""
    if n <= 0:
        raise ValueError("external_account_id expects n >= 1")
    return EXTERNAL_ACCOUNT_ID.format(n)


def is_external_account(acct: str) -> bool:
    return acct.startswith("X")
