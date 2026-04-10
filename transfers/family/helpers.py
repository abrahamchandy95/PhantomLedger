from hashlib import blake2b
from math import pow
from typing import cast

import numpy as np

import entities.models as models
from common.math import as_int
from common.persona_names import SALARIED
from common.random import Rng
from entities.personas import get_persona


def _external_family_id(person_id: str) -> str:
    """Deterministic external account ID derived from person_id."""
    h = blake2b(person_id.encode("utf-8"), digest_size=4)
    suffix = int.from_bytes(h.digest(), "little", signed=False)
    return f"XF{suffix:08d}"


def resolve_family_acct(
    person_id: str,
    primary_accounts: dict[str, str],
    external_p: float,
) -> str | None:
    """
    Resolves a family member's payment account.

    Pure function — the external/internal decision is derived
    deterministically from person_id, so the same person always
    resolves to the same account regardless of call order or
    which sub-generator calls this.
    """
    if external_p <= 0.0:
        return primary_accounts.get(person_id)

    h = blake2b(f"external_bank|{person_id}".encode("utf-8"), digest_size=8)
    val = int.from_bytes(h.digest(), "little", signed=False)
    coin = (val % 10000) / 10000.0

    if coin < external_p:
        return _external_family_id(person_id)

    return primary_accounts.get(person_id)


def pareto_amount(rng: Rng, *, xm: float, alpha: float) -> float:
    u = float(rng.float())
    scale = pow(1.0 - u, -1.0 / float(alpha))
    amount = float(xm) * float(scale)
    return round(max(float(xm), amount), 2)


def pick_education_payee(
    merchants: models.Merchants,
    gen: np.random.Generator,
) -> str | None:
    education_accounts = [
        acct
        for acct, cat in zip(merchants.counterparties, merchants.categories)
        if cat == "education"
    ]
    if not education_accounts:
        return None

    idx = as_int(cast(int | np.integer, gen.integers(0, len(education_accounts))))
    return education_accounts[idx]


def support_capacity_weight(
    person_id: str,
    persona_objects: dict[str, models.Persona],
) -> float:
    """
    Returns the financial weight of a person for capacity-based
    selection (who pays more in family support, gifts, etc.).

    Uses the individualized persona object so two salaried people
    with different weight values (e.g. 0.85 vs 1.12) contribute
    differently, rather than both returning exactly 1.0.
    """
    persona = persona_objects.get(person_id)
    if persona is not None:
        return float(persona.weight)
    # Fallback to singleton for external/unknown people
    return float(get_persona(SALARIED).weight)


def weighted_pick_person(
    people: list[str],
    persona_objects: dict[str, models.Persona],
    gen: np.random.Generator,
) -> str:
    """
    Pick one person weighted by their individualized financial capacity.
    """
    if len(people) == 1:
        return people[0]

    weights = [
        support_capacity_weight(person_id, persona_objects) for person_id in people
    ]
    total = float(sum(weights))

    if total <= 0.0:
        idx = as_int(cast(int | np.integer, gen.integers(0, len(people))))
        return people[idx]

    u = float(gen.random()) * total
    acc = 0.0
    for person_id, weight in zip(people, weights):
        acc += float(weight)
        if u <= acc:
            return person_id

    return people[-1]
