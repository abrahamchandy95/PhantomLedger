from math import pow
from typing import cast

import numpy as np

from common.math import as_int
from common.random import Rng
from entities.merchants import MerchantData


def pareto_amount(rng: Rng, *, xm: float, alpha: float) -> float:
    """
    Pareto Type I sample:
        X = xm * (1 - U)^(-1 / alpha)
    where U ~ Uniform(0, 1).
    """
    u = float(rng.float())
    scale = pow(1.0 - u, -1.0 / float(alpha))
    amount = float(xm) * float(scale)
    return round(max(float(xm), amount), 2)


def pick_education_payee(
    merchants: MerchantData,
    gen: np.random.Generator,
) -> str | None:
    education_accounts = [
        acct
        for acct, cat in zip(merchants.counterparty_acct, merchants.category)
        if cat == "education"
    ]
    if not education_accounts:
        return None

    idx = as_int(cast(int | np.integer, gen.integers(0, len(education_accounts))))
    return education_accounts[idx]


def support_capacity_weight(persona: str) -> float:
    match persona:
        case "hnw":
            return 2.2
        case "smallbiz":
            return 1.5
        case "freelancer":
            return 0.95
        case "salaried":
            return 1.0
        case _:
            return 0.75


def weighted_pick_person(
    people: list[str],
    persona_for_person: dict[str, str],
    gen: np.random.Generator,
) -> str:
    if len(people) == 1:
        return people[0]

    weights = [
        support_capacity_weight(persona_for_person.get(person_id, "salaried"))
        for person_id in people
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
