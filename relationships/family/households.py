from dataclasses import dataclass
from typing import cast

import numpy as np

from common.config import FamilyConfig
from common.math import ArrF64, ArrI64, build_cdf, cdf_pick


@dataclass(frozen=True, slots=True)
class HouseholdGraph:
    households: list[list[str]]
    household_id_for_person: dict[str, int]


def build_household_graph(
    cfg: FamilyConfig,
    gen: np.random.Generator,
    *,
    persons: list[str],
) -> HouseholdGraph:
    if not persons:
        return HouseholdGraph(
            households=[],
            household_id_for_person={},
        )

    perm: ArrI64 = np.asarray(gen.permutation(len(persons)), dtype=np.int64)
    cdf_2plus = build_household_size_cdf(
        alpha=float(cfg.household_zipf_alpha),
        max_size=int(cfg.household_max_size),
    )

    households: list[list[str]] = []
    household_id_for_person: dict[str, int] = {}

    person_index = 0
    household_id = 0
    n_people = len(persons)

    while person_index < n_people:
        size = sample_household_size(cfg, gen, cdf_2plus)
        household_end = min(n_people, person_index + size)

        household = [
            persons[int(cast(int | np.integer, perm[k]))]
            for k in range(person_index, household_end)
        ]
        households.append(household)

        for person_id in household:
            household_id_for_person[person_id] = household_id

        household_id += 1
        person_index = household_end

    return HouseholdGraph(
        households=households,
        household_id_for_person=household_id_for_person,
    )


def build_household_size_cdf(alpha: float, max_size: int) -> ArrF64:
    sizes: ArrF64 = np.arange(2, max_size + 1, dtype=np.float64)
    weights: ArrF64 = sizes ** (-float(alpha))
    return build_cdf(weights)


def sample_household_size(
    cfg: FamilyConfig,
    gen: np.random.Generator,
    cdf_2plus: ArrF64,
) -> int:
    if float(gen.random()) < float(cfg.single_household_frac):
        return 1

    picked_index = cdf_pick(cdf_2plus, float(gen.random()))
    size = 2 + picked_index
    return min(int(cfg.household_max_size), max(2, int(size)))
