from dataclasses import dataclass
from hashlib import blake2b
from typing import Protocol, cast, runtime_checkable

import pgeocode  # pyright: ignore[reportMissingTypeStubs]
from faker import Faker

from common.persona_names import (
    FREELANCER,
    HNW,
    RETIRED,
    SALARIED,
    SMALLBIZ,
    STUDENT,
)
from pipeline.state import Transfers


_DEFAULT_REFERENCE_YEAR = 2025
_ADDRESS_RETRY_LIMIT = 12

_FALLBACK_GEOS: tuple[tuple[str, str, str], ...] = (
    ("Los Angeles", "CA", "90012"),
    ("San Diego", "CA", "92101"),
    ("Houston", "TX", "77002"),
    ("Dallas", "TX", "75201"),
    ("Miami", "FL", "33130"),
    ("Tampa", "FL", "33602"),
    ("New York", "NY", "10001"),
    ("Chicago", "IL", "60601"),
    ("Phoenix", "AZ", "85004"),
    ("Philadelphia", "PA", "19107"),
    ("Atlanta", "GA", "30303"),
    ("Charlotte", "NC", "28202"),
    ("Seattle", "WA", "98101"),
    ("Boston", "MA", "02108"),
    ("Columbus", "OH", "43215"),
    ("Detroit", "MI", "48226"),
)


@runtime_checkable
class _SupportsToDict(Protocol):
    def to_dict(self) -> dict[str, object]: ...


class _PostalLookup(Protocol):
    def query_postal_code(self, code: str) -> object: ...


_US_NOMI: _PostalLookup = cast(
    _PostalLookup,
    cast(object, pgeocode.Nominatim("us")),
)


@dataclass(frozen=True, slots=True)
class _AgeBand:
    min_age: int
    max_age: int
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


def reference_year(transfers: Transfers) -> int:
    years = [txn.timestamp.year for txn in transfers.final_txns]
    if not years:
        return _DEFAULT_REFERENCE_YEAR
    return min(years)


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


def _seeded_faker(*parts: str) -> Faker:
    fake = Faker("en_US")
    fake.seed_instance(_stable_u64(*parts))
    return fake


def _clean_text(value: object) -> str:
    if value is None:
        return ""

    text = str(value).strip()
    if not text:
        return ""

    lowered = text.lower()
    if lowered in {"nan", "none"}:
        return ""

    return text


def _normalize_zipcode(raw: str) -> str:
    digits = "".join(ch for ch in raw if ch.isdigit())
    if len(digits) < 5:
        return ""
    return digits[:5]


def _geo_field(geo: object, field: str) -> str:
    if not isinstance(geo, _SupportsToDict):
        return ""

    data = geo.to_dict()
    return _clean_text(data.get(field, ""))


def _resolve_geo_from_zipcode(zipcode: str) -> tuple[str, str] | None:
    geo = _US_NOMI.query_postal_code(zipcode)

    city = _geo_field(geo, "place_name")
    state = _geo_field(geo, "state_code")

    if city and state:
        return city, state

    return None


def _fallback_geo(seed_key: str) -> tuple[str, str, str]:
    x = _stable_u64(seed_key, "identity", "geo", "fallback")
    return _FALLBACK_GEOS[x % len(_FALLBACK_GEOS)]


def _street_address(fake: Faker, seed_key: str) -> str:
    address = fake.street_address()

    if _stable_u64(seed_key, "identity", "address", "unit") % 5 == 0:
        address = f"{address} {fake.secondary_address()}"

    return address


def _fake_address_parts(seed_key: str) -> dict[str, str]:
    for attempt in range(_ADDRESS_RETRY_LIMIT):
        fake = _seeded_faker(seed_key, "identity", "address", str(attempt))

        zipcode = _normalize_zipcode(fake.postcode())
        if not zipcode:
            continue

        resolved = _resolve_geo_from_zipcode(zipcode)
        if resolved is None:
            continue

        city, state = resolved

        return {
            "address": _street_address(fake, seed_key),
            "state": state,
            "city": city,
            "zipcode": zipcode,
            "country": "US",
        }

    fallback_city, fallback_state, fallback_zip = _fallback_geo(seed_key)
    fake = _seeded_faker(seed_key, "identity", "address", "fallback")

    return {
        "address": _street_address(fake, seed_key),
        "state": fallback_state,
        "city": fallback_city,
        "zipcode": fallback_zip,
        "country": "US",
    }


def blank_identity() -> dict[str, str]:
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


def fake_identity(
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

    ssn_x = _stable_u64(seed_key, "identity", "ssn")
    ssn_a = 100 + (ssn_x % 800)
    ssn_b = 10 + ((ssn_x >> 12) % 90)
    ssn_c = 1000 + ((ssn_x >> 24) % 9000)

    addr = _fake_address_parts(seed_key)

    return {
        "name": f"Customer {seed_key[-6:]}",
        "SSN": f"{ssn_a:03d}-{ssn_b:02d}-{ssn_c:04d}",
        "dob": f"{year:04d}-{month:02d}-{day:02d}",
        "address": addr["address"],
        "state": addr["state"],
        "city": addr["city"],
        "zipcode": addr["zipcode"],
        "country": addr["country"],
    }
