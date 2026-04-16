from hashlib import blake2b

from common.persona_names import FREELANCER, HNW, SMALLBIZ

_HASH_BYTES = 8
_SUFFIX_DIGITS = 20
_SUFFIX_LIMIT = 10**_SUFFIX_DIGITS

# Business operating accounts and brokerage/custody accounts owned by a known
# person are modeled as *additional internal accounts* at the same bank,
# not as external counterparties. Chase, Bank of America, Wells Fargo, and
# peers all actively bundle personal and business banking, so a freelancer
# or small-business owner drawing funds into a personal account is normally
# an internal book-to-book transfer rather than an interbank ACH.
#
# To keep that semantics consistent with the rest of the codebase, these
# prefixes must NOT start with "X" — the repo uses a leading X to signal
# external accounts (`common.ids.is_external`, export/standard typing, the
# balance clearing house, etc.). "BOP" (business operating) and "BRK"
# (brokerage) are distinctive enough to stay greppable while behaving as
# ordinary internal accounts downstream.
_BUSINESS_PREFIX = "BOP"
_BROKERAGE_PREFIX = "BRK"


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
    Register modeled internal secondary accounts owned by known people.

    - freelancers / smallbiz: one business operating account (BOP...)
    - hnw: one brokerage / custody account (BRK...)

    These are in-bank accounts belonging to the same customer as the
    primary account. They stay attributable through owner_map/by_person
    and are NOT marked external when merged into the accounts registry.
    Owner draws from BOP to the primary account therefore show up as
    intra-customer book transfers, which matches how large U.S. retail
    banks actually settle these flows.
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
