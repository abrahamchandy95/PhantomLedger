from dataclasses import dataclass
from typing import cast

import numpy as np

from common.config import FamilyConfig


@dataclass(frozen=True, slots=True)
class SupportLinks:
    adult_parents_of: dict[str, tuple[str, ...]]
    adult_children_of: dict[str, list[str]]


def build_support_links(
    cfg: FamilyConfig,
    gen: np.random.Generator,
    *,
    households: list[list[str]],
    household_id_for_person: dict[str, int],
    spouse_of: dict[str, str],
    persons: list[str],
    persona_for_person: dict[str, str],
) -> SupportLinks:
    adult_parents_of: dict[str, tuple[str, ...]] = {}
    adult_children_of: dict[str, list[str]] = {}

    global_supporters = [
        person_id
        for person_id in persons
        if is_adult_supporter(persona_for_person, person_id)
    ]
    retirees = [
        person_id for person_id in persons if is_retired(persona_for_person, person_id)
    ]

    for retired_parent in retirees:
        spouse = spouse_of.get(retired_parent)
        if spouse is not None and is_retired(persona_for_person, spouse):
            shared_children = adult_children_of.get(spouse)
            if shared_children is not None and float(gen.random()) < 0.85:
                adult_children_of[retired_parent] = list(shared_children)
                for adult_child in shared_children:
                    existing_parents = adult_parents_of.get(adult_child, ())
                    adult_parents_of[adult_child] = existing_parents + (retired_parent,)
                continue

        if float(gen.random()) >= float(cfg.retiree_has_adult_child_p):
            continue

        household = households[household_id_for_person[retired_parent]]
        resident_supporters = [
            person_id
            for person_id in household
            if person_id != retired_parent
            and is_adult_supporter(persona_for_person, person_id)
        ]

        use_resident_supporter = bool(resident_supporters) and (
            float(gen.random()) < float(cfg.retiree_support_co_reside_p)
        )
        candidate_pool = (
            resident_supporters if use_resident_supporter else global_supporters
        )

        chosen_children = choose_unique_people(
            gen,
            candidate_pool,
            k=sample_support_children_count(gen),
            exclude={retired_parent},
        )
        if not chosen_children:
            continue

        adult_children_of[retired_parent] = list(chosen_children)
        for adult_child in chosen_children:
            existing_parents = adult_parents_of.get(adult_child, ())
            adult_parents_of[adult_child] = existing_parents + (retired_parent,)

    return SupportLinks(
        adult_parents_of=adult_parents_of,
        adult_children_of=adult_children_of,
    )


def is_retired(persona_for_person: dict[str, str], person_id: str) -> bool:
    return persona_for_person.get(person_id, "salaried") == "retired"


def is_adult_supporter(persona_for_person: dict[str, str], person_id: str) -> bool:
    return persona_for_person.get(person_id, "salaried") in {
        "salaried",
        "freelancer",
        "smallbiz",
        "hnw",
    }


def sample_support_children_count(gen: np.random.Generator) -> int:
    """
    Simple heuristic:
      - usually 1 adult child participates
      - sometimes 2
      - rarely 3
    """
    draw = float(gen.random())
    if draw < 0.65:
        return 1
    if draw < 0.92:
        return 2
    return 3


def choose_unique_people(
    gen: np.random.Generator,
    pool: list[str],
    *,
    k: int,
    exclude: set[str] | None = None,
) -> list[str]:
    if not pool or k <= 0:
        return []

    excluded_people = exclude or set()
    eligible_people = [
        person_id for person_id in pool if person_id not in excluded_people
    ]
    if not eligible_people:
        return []

    if k >= len(eligible_people):
        return list(eligible_people)

    chosen: list[str] = []
    chosen_set: set[str] = set()

    tries = 0
    max_tries = max(20, 8 * k)

    while len(chosen) < k and tries < max_tries:
        picked_index = int(
            cast(int | np.integer, gen.integers(0, len(eligible_people)))
        )
        candidate = eligible_people[picked_index]
        tries += 1
        if candidate in chosen_set:
            continue
        chosen.append(candidate)
        chosen_set.add(candidate)

    if len(chosen) < k:
        for candidate in eligible_people:
            if candidate not in chosen_set:
                chosen.append(candidate)
                chosen_set.add(candidate)
            if len(chosen) >= k:
                break

    return chosen
