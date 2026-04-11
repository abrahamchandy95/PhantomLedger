from hashlib import blake2b


def external_family_id(person_id: str) -> str:
    """Deterministic external-family account ID derived from a known person."""
    h = blake2b(person_id.encode("utf-8"), digest_size=4)
    suffix = int.from_bytes(h.digest(), "little", signed=False)
    return f"XF{suffix:08d}"


def uses_external_family_account(person_id: str, external_p: float) -> bool:
    """
    Deterministically decide whether this person uses an out-of-bank family account.

    The decision is stable for a given person_id and probability, so every generator
    sees the same family member as either internal or external without sharing state.
    """
    if external_p <= 0.0:
        return False
    if external_p >= 1.0:
        return True

    h = blake2b(f"external_bank|{person_id}".encode("utf-8"), digest_size=8)
    val = int.from_bytes(h.digest(), "little", signed=False)
    coin = (val % 10_000) / 10_000.0
    return coin < external_p


def resolve_family_acct(
    person_id: str,
    primary_accounts: dict[str, str],
    external_p: float,
) -> str | None:
    """Resolve the payment account for a known family member."""
    primary = primary_accounts.get(person_id)
    if primary is None:
        return None

    if uses_external_family_account(person_id, external_p):
        return external_family_id(person_id)

    return primary


def planned_external_family_accounts(
    person_ids: list[str],
    primary_accounts: dict[str, str],
    external_p: float,
) -> dict[str, list[str]]:
    """
    Enumerate all deterministic family external accounts that should exist.

    We only register accounts for people who already have an internal primary account.
    Returned values are shaped for entities.accounts.merge_owned_accounts().
    """
    if external_p <= 0.0:
        return {}

    planned: dict[str, list[str]] = {}
    for person_id in person_ids:
        if person_id not in primary_accounts:
            continue
        if not uses_external_family_account(person_id, external_p):
            continue
        planned[person_id] = [external_family_id(person_id)]

    return planned
