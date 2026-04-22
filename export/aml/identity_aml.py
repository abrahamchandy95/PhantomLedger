"""
AML-enriched identity generation.

Extends the mule_ml identity module with richer attributes required
by the TigerGraph AML schema: marital status, net-worth codes,
annual-income codes, occupation, risk rating, and structured
Name / Address records.

All derivations are deterministic per person_id via blake2b hashing.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timedelta
from hashlib import blake2b

from common.persona_names import (
    FREELANCER,
    HNW,
    RETIRED,
    SALARIED,
    SMALLBIZ,
    STUDENT,
)


# ── deterministic hashing ────────────────────────────────────────


def stable_u64(*parts: str) -> int:
    """Deterministic 64-bit integer from string parts.

    Public because shared.py and edges.py also need it.
    """
    h = blake2b(digest_size=8)
    for part in parts:
        h.update(part.encode("utf-8"))
        h.update(b"|")
    return int.from_bytes(h.digest(), "little", signed=False)


# ── Customer-level attributes ────────────────────────────────────

_OCCUPATION_BY_PERSONA: dict[str, tuple[str, ...]] = {
    STUDENT: ("student", "intern", "part_time"),
    RETIRED: ("retired",),
    SALARIED: ("office_worker", "engineer", "manager", "teacher", "healthcare"),
    FREELANCER: ("freelancer", "consultant", "designer", "writer"),
    SMALLBIZ: ("business_owner", "restaurateur", "contractor"),
    HNW: ("executive", "investor", "attorney", "physician"),
}

# Persona-level marriage probability.  The family graph with real
# spouse links is built inside the transfer generator and is not
# available at export time, so we approximate with Census/Pew-informed
# base rates per persona and a deterministic coin flip per person.
_MARRIED_P: dict[str, float] = {
    STUDENT: 0.05,
    RETIRED: 0.55,
    SALARIED: 0.50,
    FREELANCER: 0.35,
    SMALLBIZ: 0.55,
    HNW: 0.60,
}


def _pick(items: tuple[str, ...], seed: int) -> str:
    return items[seed % len(items)]


def marital_status(person_id: str, persona: str) -> str:
    """Deterministic marital status seeded by person_id and persona."""
    x = stable_u64(person_id, "marital")
    p = _MARRIED_P.get(persona, 0.50)
    if (x % 100) < int(p * 100):
        return "married"
    return _pick(("single", "single", "single", "divorced", "widowed"), x >> 8)


def networth_code(persona: str) -> str:
    table = {
        STUDENT: "A",
        RETIRED: "C",
        SALARIED: "B",
        FREELANCER: "B",
        SMALLBIZ: "C",
        HNW: "E",
    }
    return table.get(persona, "B")


def income_code(persona: str) -> str:
    table = {
        STUDENT: "1",
        RETIRED: "2",
        SALARIED: "3",
        FREELANCER: "3",
        SMALLBIZ: "4",
        HNW: "5",
    }
    return table.get(persona, "3")


def occupation(person_id: str, persona: str) -> str:
    pool = _OCCUPATION_BY_PERSONA.get(persona, _OCCUPATION_BY_PERSONA[SALARIED])
    x = stable_u64(person_id, "occupation")
    return _pick(pool, x)


def risk_rating(
    *,
    is_fraud: bool,
    is_mule: bool,
    is_victim: bool,
) -> str:
    if is_fraud or is_mule:
        return "very_high"
    if is_victim:
        return "high"
    return "low"


def onboarding_date(person_id: str, sim_start: datetime) -> datetime:
    x = stable_u64(person_id, "onboarding")
    days_before = 90 + (x % (365 * 5))
    return sim_start - timedelta(days=days_before)


def customer_type(persona: str) -> str:
    if persona in {SMALLBIZ}:
        return "business"
    return "individual"


# ── Name record ──────────────────────────────────────────────────

_FIRST_NAMES: tuple[str, ...] = (
    "James",
    "Mary",
    "Robert",
    "Patricia",
    "John",
    "Jennifer",
    "Michael",
    "Linda",
    "David",
    "Elizabeth",
    "William",
    "Barbara",
    "Richard",
    "Susan",
    "Joseph",
    "Jessica",
    "Thomas",
    "Sarah",
    "Charles",
    "Karen",
    "Daniel",
    "Lisa",
    "Matthew",
    "Nancy",
    "Anthony",
    "Betty",
    "Mark",
    "Margaret",
    "Donald",
    "Sandra",
    "Steven",
    "Ashley",
    "Paul",
    "Kimberly",
    "Andrew",
    "Emily",
    "Joshua",
    "Donna",
    "Kenneth",
    "Michelle",
    "Kevin",
    "Carol",
    "Brian",
    "Amanda",
    "George",
    "Dorothy",
    "Timothy",
    "Melissa",
    "Wei",
    "Priya",
    "Carlos",
    "Fatima",
    "Ahmed",
    "Yuki",
    "Dmitri",
    "Aisha",
    "Liam",
    "Sofia",
    "Mateo",
    "Olga",
)

_LAST_NAMES: tuple[str, ...] = (
    "Smith",
    "Johnson",
    "Williams",
    "Brown",
    "Jones",
    "Garcia",
    "Miller",
    "Davis",
    "Rodriguez",
    "Martinez",
    "Hernandez",
    "Lopez",
    "Gonzalez",
    "Wilson",
    "Anderson",
    "Thomas",
    "Taylor",
    "Moore",
    "Jackson",
    "Martin",
    "Lee",
    "Perez",
    "Thompson",
    "White",
    "Harris",
    "Sanchez",
    "Clark",
    "Ramirez",
    "Lewis",
    "Robinson",
    "Walker",
    "Young",
    "Allen",
    "King",
    "Wright",
    "Scott",
    "Torres",
    "Nguyen",
    "Hill",
    "Flores",
    "Green",
    "Adams",
    "Nelson",
    "Baker",
    "Hall",
    "Rivera",
    "Campbell",
    "Chen",
    "Patel",
    "Kim",
    "Muller",
    "Santos",
    "Yamamoto",
    "Khan",
    "Petrov",
    "O'Brien",
    "Johansson",
    "Rossi",
    "Dubois",
)

_MIDDLE_INITIALS: tuple[str, ...] = (
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "J",
    "K",
    "L",
    "M",
    "N",
    "P",
    "R",
    "S",
    "T",
    "W",
)


@dataclass(frozen=True, slots=True)
class NameRecord:
    id: str
    first_name: str
    middle_name: str
    last_name: str


def name_for_person(person_id: str) -> NameRecord:
    x = stable_u64(person_id, "name")
    first = _pick(_FIRST_NAMES, x)
    last = _pick(_LAST_NAMES, x >> 8)
    middle = _pick(_MIDDLE_INITIALS, x >> 16)

    return NameRecord(
        id=f"NM_{person_id}",
        first_name=first,
        middle_name=middle,
        last_name=last,
    )


def name_for_counterparty(counterparty_id: str) -> NameRecord:
    """Business-style name for an external counterparty."""
    x = stable_u64(counterparty_id, "cp_name")
    _SUFFIXES = ("LLC", "Inc", "Corp", "Ltd", "Co", "Group", "Services")
    _PREFIXES = (
        "National",
        "Pacific",
        "Eagle",
        "Summit",
        "Atlas",
        "Prime",
        "Metro",
        "Global",
        "Sterling",
        "Heritage",
        "Apex",
        "Liberty",
        "Coastal",
        "Pioneer",
        "Horizon",
        "Diamond",
        "Pinnacle",
        "Titan",
    )
    prefix = _pick(_PREFIXES, x)
    suffix = _pick(_SUFFIXES, x >> 8)
    full = f"{prefix} {suffix}"

    return NameRecord(
        id=f"NM_{counterparty_id}",
        first_name=full,
        middle_name="",
        last_name="",
    )


# ── Bank names ───────────────────────────────────────────────────

_BANK_NAMES: tuple[str, ...] = (
    "First National Bank",
    "Pacific Trust",
    "Heritage Savings",
    "Metro Credit Union",
    "Atlas Financial",
    "Liberty Bank",
    "Summit Bank",
    "Coastal Federal",
    "Pioneer Trust",
    "Sterling Savings",
)


def name_for_bank(bank_id: str) -> NameRecord:
    """Canonical name for a bank vertex.

    Used by counterparty_rows (bank_name attribute), name_rows
    (Name vertex), and bank_has_name_rows (edge) so the bank's
    identity is consistent everywhere.
    """
    x = stable_u64(bank_id, "bank_name")
    full = _pick(_BANK_NAMES, x)
    return NameRecord(
        id=f"NM_{bank_id}",
        first_name=full,
        middle_name="",
        last_name="",
    )


def routing_number_for_id(bank_id: str) -> str:
    x = stable_u64(bank_id, "routing")
    return f"{(x % 900000000) + 100000000}"


# ── Address record ───────────────────────────────────────────────

_STREETS: tuple[str, ...] = (
    "Main St",
    "Oak Ave",
    "Maple Dr",
    "Cedar Ln",
    "Elm St",
    "Pine Rd",
    "Washington Blvd",
    "Park Ave",
    "Lake Dr",
    "River Rd",
    "Hill St",
    "Sunset Blvd",
    "Broadway",
    "Market St",
    "Highland Ave",
    "Valley Rd",
    "Forest Dr",
)

_CITIES: tuple[str, ...] = (
    "New York",
    "Los Angeles",
    "Chicago",
    "Houston",
    "Phoenix",
    "Philadelphia",
    "San Antonio",
    "San Diego",
    "Dallas",
    "Austin",
    "Jacksonville",
    "San Jose",
    "Columbus",
    "Charlotte",
    "Indianapolis",
    "San Francisco",
    "Seattle",
    "Denver",
    "Nashville",
    "Portland",
    "Miami",
    "Atlanta",
    "Boston",
    "Tampa",
    "Minneapolis",
)

_STATES: tuple[str, ...] = (
    "NY",
    "CA",
    "IL",
    "TX",
    "AZ",
    "PA",
    "FL",
    "OH",
    "GA",
    "NC",
    "WA",
    "CO",
    "TN",
    "OR",
    "MN",
    "MA",
    "IN",
    "MO",
    "NV",
    "VA",
)

_ZIPS: tuple[str, ...] = (
    "10001",
    "90012",
    "60601",
    "77002",
    "85004",
    "19107",
    "33130",
    "43215",
    "30303",
    "28202",
    "98101",
    "80202",
    "37203",
    "97201",
    "55401",
    "02108",
    "46204",
    "63101",
    "89101",
    "22201",
)


@dataclass(frozen=True, slots=True)
class AddressRecord:
    id: str
    street_line1: str
    street_line2: str
    city: str
    state: str
    postal_code: str
    country: str
    address_type: str
    is_high_risk_geo: bool


def _build_address(
    seed_key: str,
    namespace: str,
    *,
    address_type: str,
) -> AddressRecord:
    x = stable_u64(seed_key, namespace)
    street_num = 100 + (x % 9900)
    street = _pick(_STREETS, x >> 8)
    city_idx = (x >> 16) % len(_CITIES)
    state_idx = city_idx % len(_STATES)
    zip_idx = city_idx % len(_ZIPS)

    has_unit = (x >> 24) % 5 == 0
    line2 = f"Apt {1 + ((x >> 28) % 500)}" if has_unit else ""

    return AddressRecord(
        id=f"ADDR_{seed_key}",
        street_line1=f"{street_num} {street}",
        street_line2=line2,
        city=_CITIES[city_idx],
        state=_STATES[state_idx],
        postal_code=_ZIPS[zip_idx],
        country="US",
        address_type=address_type,
        is_high_risk_geo=False,
    )


def address_for_person(person_id: str) -> AddressRecord:
    return _build_address(person_id, "address", address_type="residential")


def address_for_counterparty(counterparty_id: str) -> AddressRecord:
    return _build_address(counterparty_id, "cp_address", address_type="commercial")


def address_for_bank(bank_id: str) -> AddressRecord:
    return _build_address(bank_id, "bank_address", address_type="commercial")
