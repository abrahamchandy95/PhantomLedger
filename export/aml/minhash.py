"""
MinHash signatures for entity resolution.

TigerGraph's AML schema uses MinHash vertices for locality-sensitive
hashing (LSH) of names and addresses. This enables approximate
nearest-neighbor lookups: customers/counterparties with similar names
or addresses share MinHash signature nodes in the graph, creating
edges that entity-resolution queries can traverse.

Implementation:
- We compute k-shingle sets from the text fields
- Apply a family of hash functions (simulated via blake2b with salts)
- Take the minimum hash per function to form the signature
- Quantize each signature element into a bucket ID

The bucket IDs become vertex IDs in the MinHash vertex types. Two
entities sharing a bucket for any signature position are candidate
matches.
"""

from __future__ import annotations

from hashlib import blake2b

_NUM_HASHES = 8
_SHINGLE_K = 3
_BUCKET_SIZE = 1000


def _shingles(text: str, k: int = _SHINGLE_K) -> set[str]:
    """Extract character k-shingles from normalized text."""
    t = text.lower().strip()
    if len(t) < k:
        return {t} if t else set()
    return {t[i : i + k] for i in range(len(t) - k + 1)}


def _hash_shingle(shingle: str, salt: int) -> int:
    h = blake2b(
        f"{salt}|{shingle}".encode("utf-8"),
        digest_size=8,
    )
    return int.from_bytes(h.digest(), "little", signed=False)


def _minhash_signature(text: str) -> tuple[int, ...]:
    """Compute a MinHash signature vector of length _NUM_HASHES."""
    shings = _shingles(text)
    if not shings:
        return tuple(0 for _ in range(_NUM_HASHES))

    sig: list[int] = []
    for salt in range(_NUM_HASHES):
        min_h = min(_hash_shingle(s, salt) for s in shings)
        sig.append(min_h % _BUCKET_SIZE)
    return tuple(sig)


def _bucket_ids(prefix: str, signature: tuple[int, ...]) -> list[str]:
    """Convert a signature into bucket vertex IDs."""
    return [f"{prefix}_{i}_{signature[i]}" for i in range(len(signature))]


# ── Public API ───────────────────────────────────────────────────


def name_minhash_ids(first_name: str, last_name: str) -> list[str]:
    """Bucket IDs for Name_MinHash vertices."""
    full = f"{first_name} {last_name}".strip()
    sig = _minhash_signature(full)
    return _bucket_ids("NMH", sig)


def address_minhash_ids(full_address: str) -> list[str]:
    """Bucket IDs for Address_MinHash vertices."""
    sig = _minhash_signature(full_address)
    return _bucket_ids("AMH", sig)


def street_minhash_ids(street_line1: str) -> list[str]:
    """Bucket IDs for Street_Line1_MinHash vertices."""
    sig = _minhash_signature(street_line1)
    return _bucket_ids("SMH", sig)


def city_minhash_id(city: str) -> str:
    """Single bucket ID for City_MinHash (exact normalized match)."""
    return f"CMH_{city.lower().strip().replace(' ', '_')}"


def state_minhash_id(state: str) -> str:
    """Single bucket ID for State_MinHash (exact normalized match)."""
    return f"STMH_{state.upper().strip()}"
