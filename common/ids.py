from collections.abc import Iterator
from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True, slots=True)
class IdFormat:
    prefix: str
    width: int

    def apply(self, n: int) -> str:
        if n <= 0:
            raise ValueError(f"ID sequence must be >= 1, got {n}")
        return f"{self.prefix}{n:0{self.width}d}"


PERSON = IdFormat(prefix="C", width=10)
ACCOUNT = IdFormat(prefix="A", width=10)

MERCHANT = IdFormat(prefix="M", width=8)
MERCHANT_EXTERNAL = IdFormat(prefix="XM", width=8)

EMPLOYER_EXTERNAL = IdFormat(prefix="XE", width=8)
LANDLORD_EXTERNAL = IdFormat(prefix="XL", width=8)
CLIENT_EXTERNAL = IdFormat(prefix="XC", width=8)
PLATFORM_EXTERNAL = IdFormat(prefix="XP", width=8)
PROCESSOR_EXTERNAL = IdFormat(prefix="XS", width=8)
BUSINESS_EXTERNAL = IdFormat(prefix="XO", width=8)
BROKERAGE_EXTERNAL = IdFormat(prefix="XB", width=8)


def customer_id(n: int) -> str:
    return PERSON.apply(n)


def account_id(n: int) -> str:
    return ACCOUNT.apply(n)


def employer_external_id(n: int) -> str:
    return EMPLOYER_EXTERNAL.apply(n)


def merchant_id(n: int) -> str:
    return MERCHANT.apply(n)


def merchant_external_id(n: int) -> str:
    return MERCHANT_EXTERNAL.apply(n)


def landlord_external_id(n: int) -> str:
    return LANDLORD_EXTERNAL.apply(n)


def client_external_id(n: int) -> str:
    return CLIENT_EXTERNAL.apply(n)


def platform_external_id(n: int) -> str:
    return PLATFORM_EXTERNAL.apply(n)


def processor_external_id(n: int) -> str:
    return PROCESSOR_EXTERNAL.apply(n)


def business_external_id(n: int) -> str:
    return BUSINESS_EXTERNAL.apply(n)


def brokerage_external_id(n: int) -> str:
    return BROKERAGE_EXTERNAL.apply(n)


def customers(count: int) -> Iterator[str]:
    """Yield Cxxxxxxx ids from 1 to count."""
    for i in range(1, count + 1):
        yield customer_id(i)


def accounts(count: int) -> Iterator[str]:
    """Yield Axxxxxxxxx ids from 1 to count."""
    for i in range(1, count + 1):
        yield account_id(i)


def device_id(cust_id: str, index: int) -> str:
    """
    Deterministic device ID linked to a customer.
    Example: C0000123, index 1 -> D0000123_1
    """
    if index <= 0:
        raise ValueError(f"Device index must be >= 1, got {index}")

    return f"D{cust_id[1:]}_{index}"


def fraud_device_id(ring_id: int) -> str:
    """Shared fraud ring device: FD0000, FD0001..."""
    if ring_id < 0:
        raise ValueError(f"Ring ID must be >= 0, got {ring_id}")
    return f"FD{ring_id:04d}"


def random_ip(rng: np.random.Generator) -> str:
    o1 = int(rng.integers(11, 223, dtype=np.int64))
    o2 = int(rng.integers(0, 256, dtype=np.int64))
    o3 = int(rng.integers(0, 256, dtype=np.int64))
    o4 = int(rng.integers(1, 255, dtype=np.int64))
    return f"{o1}.{o2}.{o3}.{o4}"


def is_external(acct_id: str) -> bool:
    """Check if an account ID belongs to an external counterparty."""
    return acct_id.startswith("X")
