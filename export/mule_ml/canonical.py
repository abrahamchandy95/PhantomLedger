from collections import Counter, defaultdict
from collections.abc import Iterable

from pipeline.state import Entities, Infra, Transfers


def _stable_u64(*parts: str) -> int:
    from hashlib import blake2b

    h = blake2b(digest_size=8)
    for part in parts:
        h.update(part.encode("utf-8"))
        h.update(b"|")
    return int.from_bytes(h.digest(), "little", signed=False)


def _fallback_ip(seed_key: str) -> str:
    x = _stable_u64(seed_key, "ip")
    return (
        f"{11 + (x % 212)}.{(x >> 8) % 256}.{(x >> 16) % 256}.{1 + ((x >> 24) % 254)}"
    )


def _fallback_device(account_id: str) -> str:
    return f"DEV_{account_id}"


def _pick_canonical(
    seen: Counter[str], fallbacks: Iterable[str], *, default: str
) -> str:
    if seen:
        winner, _ = sorted(seen.items(), key=lambda item: (-item[1], item[0]))[0]
        return winner
    for value in fallbacks:
        if value:
            return value
    return default


def canonical_maps(
    entities: Entities,
    infra: Infra,
    transfers: Transfers,
    party_ids: set[str],
) -> tuple[dict[str, str], dict[str, str]]:
    device_counts: defaultdict[str, Counter[str]] = defaultdict(Counter)
    ip_counts: defaultdict[str, Counter[str]] = defaultdict(Counter)

    for txn in transfers.final_txns:
        if txn.device_id:
            device_counts[txn.source][txn.device_id] += 1
        if txn.ip_address:
            ip_counts[txn.source][txn.ip_address] += 1

    acct_to_device: dict[str, str] = {}
    acct_to_ip: dict[str, str] = {}

    for account_id in party_ids:
        owner = entities.accounts.owner_map.get(account_id)

        fallback_devices = infra.devices.by_person.get(owner, []) if owner else []
        fallback_ips = infra.ips.by_person.get(owner, []) if owner else []

        acct_to_device[account_id] = _pick_canonical(
            device_counts[account_id],
            fallback_devices,
            default=_fallback_device(account_id),
        )
        acct_to_ip[account_id] = _pick_canonical(
            ip_counts[account_id],
            fallback_ips,
            default=_fallback_ip(account_id),
        )

    return acct_to_device, acct_to_ip
