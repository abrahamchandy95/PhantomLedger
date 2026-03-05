import hashlib


def derived_seed(base_seed: int, *parts: str) -> int:
    """
    Deterministic seed derived from base_seed + parts.
    Produces a 64-bit int suitable for np.random.default_rng.
    """
    h = hashlib.blake2b(digest_size=8)
    h.update(str(base_seed).encode("utf-8"))
    for p in parts:
        h.update(b"|")
        h.update(p.encode("utf-8"))
    return int.from_bytes(h.digest(), "little", signed=False)
