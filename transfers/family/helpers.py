from math import pow
from typing import cast

import numpy as np

import entities.models as models
from common.math import as_int
from common.persona_names import SALARIED
from common.random import Rng
from entities.personas import get_persona


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
        for acct, cat in zip(
            merchants.counterparties, merchants.categories, strict=True
        )
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
    persona = persona_objects.get(person_id)
    if persona is not None:
        return float(persona.weight)

    return float(get_persona(SALARIED).weight)


def weighted_pick_person(
    people: list[str],
    persona_objects: dict[str, models.Persona],
    gen: np.random.Generator,
) -> str:
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
    for person_id, weight in zip(people, weights, strict=True):
        acc += float(weight)
        if u <= acc:
            return person_id

    return people[-1]
