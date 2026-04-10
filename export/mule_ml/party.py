from collections import Counter, defaultdict
from collections.abc import Iterable, Iterator
from hashlib import blake2b

from common.schema import ML_PARTY
from pipeline.state import Entities, Infra, Transfers
from ..csv_io import Row


HEADER = ML_PARTY.header


def _stable_u64(*parts: str) -> int:
    h = blake2b(digest_size=8)
    for part in parts:
        h.update(part.encode("utf-8"))
        h.update(b"|")
    return int.from_bytes(h.digest(), "little", signed=False)


def _fake_identity(seed_key: str) -> dict[str, str]:
    x = _stable_u64(seed_key, "identity")

    year = 1955 + (x % 40)
    month = 1 + ((x >> 8) % 12)
    day = 1 + ((x >> 16) % 28)
    zipcode = 90001 + ((x >> 24) % 900)

    ssn_a = 100 + (x % 800)
    ssn_b = 10 + ((x >> 12) % 90)
    ssn_c = 1000 + ((x >> 24) % 9000)

    return {
        "name": f"Customer {seed_key[-6:]}",
        "SSN": f"{ssn_a:03d}-{ssn_b:02d}-{ssn_c:04d}",
        "dob": f"{year:04d}-{month:02d}-{day:02d}",
        "address": f"{1 + (x % 9999)} Example Street",
        "state": "CA",
        "city": "San Francisco",
        "zipcode": str(zipcode),
        "country": "US",
    }


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


def all_party_ids(entities: Entities, transfers: Transfers) -> set[str]:
    ids = set(entities.accounts.ids)
    for txn in transfers.final_txns:
        ids.add(txn.source)
        ids.add(txn.target)
    return ids


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
            ip_counts[account_id], fallback_ips, default=_fallback_ip(account_id)
        )

    return acct_to_device, acct_to_ip


def rows(
    entities: Entities,
    infra: Infra,
    transfers: Transfers,
) -> Iterator[Row]:
    party_ids = all_party_ids(entities, transfers)
    acct_to_device, acct_to_ip = canonical_maps(entities, infra, transfers, party_ids)

    mules = entities.accounts.mules
    orgs = entities.accounts.frauds
    victims = entities.accounts.victims
    solos = entities.people.solo_frauds

    for account_id in sorted(party_ids):
        owner = entities.accounts.owner_map.get(account_id)
        is_internal = owner is not None

        seed_key = owner if is_internal else account_id
        identity = _fake_identity(seed_key)

        phone = entities.pii.phone_map.get(owner, "") if is_internal else ""
        email = entities.pii.email_map.get(owner, "") if is_internal else ""

        is_mule = int(account_id in mules)
        is_organizer = int(account_id in orgs)
        is_victim = int(account_id in victims)
        is_solo = int(is_internal and owner in solos)
        is_fraud = int(bool(is_mule or is_organizer or is_solo))

        yield (
            account_id,
            is_fraud,
            is_mule,
            is_organizer,
            is_victim,
            is_solo,
            phone,
            email,
            identity["name"],
            identity["SSN"],
            identity["dob"],
            identity["address"],
            identity["state"],
            identity["city"],
            identity["zipcode"],
            identity["country"],
            acct_to_ip[account_id],
            acct_to_device[account_id],
        )
