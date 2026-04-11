from collections import Counter, defaultdict
from collections.abc import Iterable, Iterator
from dataclasses import dataclass
from hashlib import blake2b

from common.persona_names import FREELANCER, HNW, RETIRED, SALARIED, SMALLBIZ, STUDENT
from common.schema import ML_PARTY
from pipeline.state import Entities, Infra, Transfers
from ..csv_io import Row


HEADER = ML_PARTY.header
_DEFAULT_REFERENCE_YEAR = 2025


def _reference_year(transfers: Transfers) -> int:
    years = [txn.timestamp.year for txn in transfers.final_txns]
    if not years:
        return _DEFAULT_REFERENCE_YEAR
    return min(years)


@dataclass(frozen=True, slots=True)
class _AgeBand:
    min_age: int
    max_age: int
    weight: int


@dataclass(frozen=True, slots=True)
class _LocationProfile:
    city: str
    state: str
    zipcode: str
    weight: int


_PERSONA_AGE_BANDS: dict[str, tuple[_AgeBand, ...]] = {
    STUDENT: (
        _AgeBand(min_age=16, max_age=17, weight=1),
        _AgeBand(min_age=18, max_age=24, weight=7),
        _AgeBand(min_age=25, max_age=29, weight=2),
        _AgeBand(min_age=30, max_age=34, weight=1),
    ),
    RETIRED: (
        _AgeBand(min_age=65, max_age=69, weight=4),
        _AgeBand(min_age=70, max_age=79, weight=5),
        _AgeBand(min_age=80, max_age=89, weight=3),
        _AgeBand(min_age=90, max_age=99, weight=1),
    ),
    FREELANCER: (
        _AgeBand(min_age=21, max_age=29, weight=2),
        _AgeBand(min_age=30, max_age=44, weight=4),
        _AgeBand(min_age=45, max_age=59, weight=4),
        _AgeBand(min_age=60, max_age=74, weight=2),
    ),
    SMALLBIZ: (
        _AgeBand(min_age=25, max_age=34, weight=1),
        _AgeBand(min_age=35, max_age=49, weight=3),
        _AgeBand(min_age=50, max_age=64, weight=4),
        _AgeBand(min_age=65, max_age=79, weight=2),
    ),
    HNW: (
        _AgeBand(min_age=30, max_age=44, weight=2),
        _AgeBand(min_age=45, max_age=59, weight=4),
        _AgeBand(min_age=60, max_age=74, weight=4),
        _AgeBand(min_age=75, max_age=85, weight=2),
    ),
    SALARIED: (
        _AgeBand(min_age=22, max_age=29, weight=2),
        _AgeBand(min_age=30, max_age=44, weight=4),
        _AgeBand(min_age=45, max_age=54, weight=3),
        _AgeBand(min_age=55, max_age=67, weight=2),
    ),
}

_DEFAULT_AGE_BANDS = _PERSONA_AGE_BANDS[SALARIED]

_LOCATION_PROFILES: tuple[_LocationProfile, ...] = (
    _LocationProfile(city="Los Angeles", state="CA", zipcode="90012", weight=14),
    _LocationProfile(city="San Diego", state="CA", zipcode="92101", weight=5),
    _LocationProfile(city="Houston", state="TX", zipcode="77002", weight=9),
    _LocationProfile(city="Dallas", state="TX", zipcode="75201", weight=8),
    _LocationProfile(city="Miami", state="FL", zipcode="33130", weight=7),
    _LocationProfile(city="Tampa", state="FL", zipcode="33602", weight=5),
    _LocationProfile(city="New York", state="NY", zipcode="10001", weight=8),
    _LocationProfile(city="Chicago", state="IL", zipcode="60601", weight=6),
    _LocationProfile(city="Phoenix", state="AZ", zipcode="85004", weight=4),
    _LocationProfile(city="Philadelphia", state="PA", zipcode="19107", weight=5),
    _LocationProfile(city="Atlanta", state="GA", zipcode="30303", weight=5),
    _LocationProfile(city="Charlotte", state="NC", zipcode="28202", weight=4),
    _LocationProfile(city="Seattle", state="WA", zipcode="98101", weight=4),
    _LocationProfile(city="Boston", state="MA", zipcode="02108", weight=3),
    _LocationProfile(city="Columbus", state="OH", zipcode="43215", weight=4),
    _LocationProfile(city="Detroit", state="MI", zipcode="48226", weight=3),
)

_STREETS: tuple[str, ...] = (
    "Maple Street",
    "Oak Avenue",
    "Pine Road",
    "Cedar Lane",
    "Lakeview Drive",
    "River Street",
    "Hillcrest Avenue",
    "Park Boulevard",
    "Washington Street",
    "Lincoln Avenue",
    "Jefferson Drive",
    "Adams Street",
    "Sunset Road",
    "Meadow Lane",
    "Highland Avenue",
    "Walnut Street",
    "Chestnut Drive",
    "Sycamore Road",
    "Willow Avenue",
    "Spruce Street",
)


def _stable_u64(*parts: str) -> int:
    h = blake2b(digest_size=8)
    for part in parts:
        h.update(part.encode("utf-8"))
        h.update(b"|")
    return int.from_bytes(h.digest(), "little", signed=False)


def _weighted_index(weights: tuple[int, ...], x: int) -> int:
    total = sum(weights)
    if total <= 0:
        raise ValueError("weights must sum to a positive value")

    target = x % total
    acc = 0
    for idx, weight in enumerate(weights):
        acc += weight
        if target < acc:
            return idx

    return len(weights) - 1


def _synthetic_birth_day(seed_key: str) -> int:
    x = _stable_u64(seed_key, "identity")
    return 1 + ((x >> 16) % 28)


def _pick_age(seed_key: str, persona: str | None) -> int:
    bands = _PERSONA_AGE_BANDS.get(persona or "", _DEFAULT_AGE_BANDS)
    weights = tuple(b.weight for b in bands)
    x = _stable_u64(seed_key, "identity", "age")
    band = bands[_weighted_index(weights, x)]

    span = band.max_age - band.min_age + 1
    return band.min_age + ((x >> 16) % span)


def _pick_location(seed_key: str) -> _LocationProfile:
    weights = tuple(profile.weight for profile in _LOCATION_PROFILES)
    x = _stable_u64(seed_key, "identity", "location")
    return _LOCATION_PROFILES[_weighted_index(weights, x)]


def _street_address(seed_key: str) -> str:
    x = _stable_u64(seed_key, "identity", "street")
    number = 100 + (x % 9900)
    street = _STREETS[(x >> 16) % len(_STREETS)]
    return f"{number} {street}"


def _blank_identity() -> dict[str, str]:
    return {
        "name": "",
        "SSN": "",
        "dob": "",
        "address": "",
        "state": "",
        "city": "",
        "zipcode": "",
        "country": "",
    }


def _fake_identity(
    seed_key: str,
    *,
    persona: str | None = None,
    reference_year: int = _DEFAULT_REFERENCE_YEAR,
) -> dict[str, str]:
    x = _stable_u64(seed_key, "identity")

    age = _pick_age(seed_key, persona)
    year = max(1920, int(reference_year) - age)
    month = 1 + ((x >> 8) % 12)
    day = _synthetic_birth_day(seed_key)

    location = _pick_location(seed_key)

    ssn_x = _stable_u64(seed_key, "identity", "ssn")
    ssn_a = 100 + (ssn_x % 800)
    ssn_b = 10 + ((ssn_x >> 12) % 90)
    ssn_c = 1000 + ((ssn_x >> 24) % 9000)

    return {
        "name": f"Customer {seed_key[-6:]}",
        "SSN": f"{ssn_a:03d}-{ssn_b:02d}-{ssn_c:04d}",
        "dob": f"{year:04d}-{month:02d}-{day:02d}",
        "address": _street_address(seed_key),
        "state": location.state,
        "city": location.city,
        "zipcode": location.zipcode,
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
    reference_year = _reference_year(transfers)

    mules = entities.accounts.mules
    orgs = entities.accounts.frauds
    victims = entities.accounts.victims
    solos = entities.people.solo_frauds

    for account_id in sorted(party_ids):
        owner = entities.accounts.owner_map.get(account_id)
        is_internal = owner is not None

        if is_internal:
            identity = _fake_identity(
                owner,
                persona=entities.persona_map.get(owner),
                reference_year=reference_year,
            )
        else:
            identity = _blank_identity()

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
