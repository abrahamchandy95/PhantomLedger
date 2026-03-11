from dataclasses import dataclass
from typing import cast

import numpy as np

from common.config import FamilyConfig


@dataclass(frozen=True, slots=True)
class HouseholdLinks:
    spouse_of: dict[str, str]
    parents_of: dict[str, tuple[str, ...]]
    children_of: dict[str, list[str]]


def build_household_links(
    cfg: FamilyConfig,
    gen: np.random.Generator,
    *,
    households: list[list[str]],
    persons: list[str],
    persona_for_person: dict[str, str],
) -> HouseholdLinks:
    spouse_of: dict[str, str] = {}
    parents_of: dict[str, tuple[str, ...]] = {}
    children_of: dict[str, list[str]] = {}

    adults = [
        person_id
        for person_id in persons
        if not is_student(persona_for_person, person_id)
    ]
    if not adults:
        adults = list(persons)

    for household in households:
        adult_candidates = [
            person_id
            for person_id in household
            if not is_student(persona_for_person, person_id)
        ]

        if len(adult_candidates) >= 2 and float(gen.random()) < float(
            cfg.spouse_pair_p
        ):
            first_index = int(
                cast(int | np.integer, gen.integers(0, len(adult_candidates)))
            )
            second_index = int(
                cast(int | np.integer, gen.integers(0, len(adult_candidates) - 1))
            )
            if second_index >= first_index:
                second_index += 1

            first_person = adult_candidates[first_index]
            second_person = adult_candidates[second_index]
            spouse_of[first_person] = second_person
            spouse_of[second_person] = first_person

        students = [
            person_id
            for person_id in household
            if is_student(persona_for_person, person_id)
        ]
        if not students:
            continue

        for child_id in students:
            if float(gen.random()) >= float(cfg.student_dependent_p):
                continue

            use_household_parents = (len(adult_candidates) >= 1) and (
                float(gen.random()) < float(cfg.co_reside_student_p)
            )
            parent_pool = adult_candidates if use_household_parents else adults
            if not parent_pool:
                continue

            wants_two_parents = (len(parent_pool) >= 2) and (
                float(gen.random()) < float(cfg.two_parent_p)
            )

            if wants_two_parents:
                first_parent = parent_pool[
                    int(cast(int | np.integer, gen.integers(0, len(parent_pool))))
                ]
                second_parent = first_parent

                tries = 0
                while second_parent == first_parent and tries < 8:
                    second_parent = parent_pool[
                        int(cast(int | np.integer, gen.integers(0, len(parent_pool))))
                    ]
                    tries += 1

                if second_parent == first_parent and len(parent_pool) >= 2:
                    current_index = parent_pool.index(first_parent)
                    second_parent = parent_pool[(current_index + 1) % len(parent_pool)]

                parents = (
                    (first_parent, second_parent)
                    if first_parent != second_parent
                    else (first_parent,)
                )
            else:
                first_parent = parent_pool[
                    int(cast(int | np.integer, gen.integers(0, len(parent_pool))))
                ]
                parents = (first_parent,)

            parents_of[child_id] = parents
            for parent_id in parents:
                children_of.setdefault(parent_id, []).append(child_id)

    return HouseholdLinks(
        spouse_of=spouse_of,
        parents_of=parents_of,
        children_of=children_of,
    )


def is_student(persona_for_person: dict[str, str], person_id: str) -> bool:
    return persona_for_person.get(person_id, "salaried") == "student"
