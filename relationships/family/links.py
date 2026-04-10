from dataclasses import dataclass
from typing import cast

import numpy as np

from common import config
from common.persona_names import STUDENT, SALARIED


@dataclass(frozen=True, slots=True)
class Links:
    """Intermediate state holding the immediate family connections."""

    spouses: dict[str, str]
    parents: dict[str, tuple[str, ...]]
    children: dict[str, list[str]]


def _is_student(persona_map: dict[str, str], person_id: str) -> bool:
    return persona_map.get(person_id, SALARIED) == STUDENT


def build(
    cfg: config.Family,
    gen: np.random.Generator,
    *,
    households: list[list[str]],
    people: list[str],
    persona_map: dict[str, str],
) -> Links:
    """
    Builds the immediate family links: spouses and dependent children.
    """
    spouses: dict[str, str] = {}
    parents: dict[str, tuple[str, ...]] = {}
    children: dict[str, list[str]] = {}

    global_adults = [p for p in people if not _is_student(persona_map, p)]
    if not global_adults:
        global_adults = list(people)

    for hh in households:
        # Partition the household
        adults: list[str] = []
        students: list[str] = []
        for person_id in hh:
            if _is_student(persona_map, person_id):
                students.append(person_id)
            else:
                adults.append(person_id)

        if len(adults) >= 2 and float(gen.random()) < float(cfg.spouse_p):
            idx1, idx2 = cast(
                list[int], gen.choice(len(adults), size=2, replace=False).tolist()
            )
            p1, p2 = adults[idx1], adults[idx2]

            spouses[p1] = p2
            spouses[p2] = p1

        if not students:
            continue

        for child in students:
            if float(gen.random()) >= float(cfg.student_dependent_p):
                continue

            use_local = (len(adults) >= 1) and (
                float(gen.random()) < float(cfg.student_coresides_p)
            )
            pool = adults if use_local else global_adults

            if not pool:
                continue

            wants_two = (len(pool) >= 2) and (
                float(gen.random()) < float(cfg.two_parent_p)
            )

            if wants_two:
                idx1, idx2 = cast(
                    list[int], gen.choice(len(pool), size=2, replace=False).tolist()
                )
                chosen_parents = (pool[idx1], pool[idx2])
            else:
                [idx] = cast(list[int], gen.choice(len(pool), size=1).tolist())
                chosen_parents = (pool[idx],)

            parents[child] = chosen_parents
            for parent_id in chosen_parents:
                children.setdefault(parent_id, []).append(child)

    return Links(
        spouses=spouses,
        parents=parents,
        children=children,
    )
