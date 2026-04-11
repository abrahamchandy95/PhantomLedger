from hashlib import blake2b

from common.persona_names import FREELANCER, HNW, SMALLBIZ

_HASH_BYTES = 8
_SUFFIX_DIGITS = 20
_SUFFIX_LIMIT = 10**_SUFFIX_DIGITS

_BUSINESS_PREFIX = "XOB"
_BROKERAGE_PREFIX = "XBR"


def _stable_suffix(namespace: str, person_id: str) -> int:
    h = blake2b(
        f"{namespace}|{person_id}".encode("utf-8"),
        digest_size=_HASH_BYTES,
    )
    suffix = int.from_bytes(h.digest(), "little", signed=False)

    if suffix >= _SUFFIX_LIMIT:
        raise ValueError(
            "income-account hash suffix exceeded fixed-width decimal budget"
        )

    return suffix


def business_operating_account_id(person_id: str) -> str:
    suffix = _stable_suffix("business_operating", person_id)
    return f"{_BUSINESS_PREFIX}{suffix:0{_SUFFIX_DIGITS}d}"


def brokerage_custody_account_id(person_id: str) -> str:
    suffix = _stable_suffix("brokerage_custody", person_id)
    return f"{_BROKERAGE_PREFIX}{suffix:0{_SUFFIX_DIGITS}d}"


def is_business_operating_account(account_id: str) -> bool:
    return account_id.startswith(_BUSINESS_PREFIX)


def is_brokerage_custody_account(account_id: str) -> bool:
    return account_id.startswith(_BROKERAGE_PREFIX)


def planned_owned_income_accounts(
    person_ids: list[str],
    persona_for_person: dict[str, str],
    primary_accounts: dict[str, str],
) -> dict[str, list[str]]:
    """
    Register modeled external accounts that are still owned by known people.

    - freelancers / smallbiz: one external business operating account
    - hnw: one external brokerage/custody account

    These are visible in the bank graph as represented external accounts but
    remain attributable to the known person through owner_map/by_person.
    """
    planned: dict[str, list[str]] = {}

    for person_id in person_ids:
        if person_id not in primary_accounts:
            continue

        persona = persona_for_person.get(person_id, "")
        account_ids: list[str] = []

        if persona in {FREELANCER, SMALLBIZ}:
            account_ids.append(business_operating_account_id(person_id))
        elif persona == HNW:
            account_ids.append(brokerage_custody_account_id(person_id))

        if account_ids:
            planned[person_id] = account_ids

    return planned
