from dataclasses import dataclass
from typing import cast

import numpy as np
import numpy.typing as npt

from common.config import FamilyConfig
from common.probability import build_cdf, cdf_pick
from common.rng import Rng
from common.seeding import derived_seed


type ArrF64 = npt.NDArray[np.float64]
type ArrI64 = npt.NDArray[np.int64]


@dataclass(frozen=True, slots=True)
class FamilyData:
    households: list[list[str]]
    household_id_for_person: dict[str, int]

    spouse_of: dict[str, str]  # p -> spouse

    # Dependent-child relationships
    parents_of: dict[str, tuple[str, ...]]  # dependent child -> parents
    children_of: dict[str, list[str]]  # parent -> dependent children

    # Adult-child support relationships for retirees
    adult_parents_of: dict[str, tuple[str, ...]]  # adult child -> retired parent(s)
    adult_children_of: dict[str, list[str]]  # retired parent -> adult child(ren)


def _zipf_cdf(alpha: float, max_size: int) -> ArrF64:
    sizes: ArrF64 = np.arange(2, max_size + 1, dtype=np.float64)
    weights: ArrF64 = sizes ** (-float(alpha))
    return build_cdf(weights)


def _sample_household_size(
    cfg: FamilyConfig,
    gen: np.random.Generator,
    cdf_2plus: ArrF64,
) -> int:
    if float(gen.random()) < float(cfg.single_household_frac):
        return 1

    j = cdf_pick(cdf_2plus, float(gen.random()))
    size = 2 + j
    return min(int(cfg.household_max_size), max(2, int(size)))


def _is_student(persona_for_person: dict[str, str], person_id: str) -> bool:
    return persona_for_person.get(person_id, "salaried") == "student"


def _is_retired(persona_for_person: dict[str, str], person_id: str) -> bool:
    return persona_for_person.get(person_id, "salaried") == "retired"


def _is_adult_supporter(persona_for_person: dict[str, str], person_id: str) -> bool:
    return persona_for_person.get(person_id, "salaried") in {
        "salaried",
        "freelancer",
        "smallbiz",
        "hnw",
    }


def _sample_support_children_n(gen: np.random.Generator) -> int:
    """
    Simple heuristic:
      - usually 1 adult child participates
      - sometimes 2
      - rarely 3
    """
    u = float(gen.random())
    if u < 0.65:
        return 1
    if u < 0.92:
        return 2
    return 3


def _choose_unique_people(
    gen: np.random.Generator,
    pool: list[str],
    *,
    k: int,
    exclude: set[str] | None = None,
) -> list[str]:
    if not pool or k <= 0:
        return []

    excluded = exclude or set()
    eligible = [p for p in pool if p not in excluded]
    if not eligible:
        return []

    if k >= len(eligible):
        return list(eligible)

    chosen: list[str] = []
    chosen_set: set[str] = set()

    tries = 0
    max_tries = max(20, 8 * k)

    while len(chosen) < k and tries < max_tries:
        idx = int(cast(int | np.integer, gen.integers(0, len(eligible))))
        cand = eligible[idx]
        tries += 1
        if cand in chosen_set:
            continue
        chosen.append(cand)
        chosen_set.add(cand)

    if len(chosen) < k:
        for cand in eligible:
            if cand not in chosen_set:
                chosen.append(cand)
                chosen_set.add(cand)
            if len(chosen) >= k:
                break

    return chosen


def generate_family(
    cfg: FamilyConfig,
    rng: Rng,
    *,
    base_seed: int,
    persons: list[str],
    persona_for_person: dict[str, str],
) -> FamilyData:
    if not persons:
        return FamilyData([], {}, {}, {}, {}, {}, {})

    # local RNG so family structure stays stable regardless of upstream iteration order
    gen = np.random.default_rng(derived_seed(base_seed, "family", "households"))

    perm: ArrI64 = np.asarray(gen.permutation(len(persons)), dtype=np.int64)
    cdf_2plus = _zipf_cdf(float(cfg.household_zipf_alpha), int(cfg.household_max_size))

    households: list[list[str]] = []
    household_id_for_person: dict[str, int] = {}

    i = 0
    hid = 0
    n = len(persons)

    while i < n:
        size = _sample_household_size(cfg, gen, cdf_2plus)
        j = min(n, i + size)

        hh = [persons[int(cast(int | np.integer, perm[k]))] for k in range(i, j)]
        households.append(hh)

        for person_id in hh:
            household_id_for_person[person_id] = hid

        hid += 1
        i = j

    spouse_of: dict[str, str] = {}
    parents_of: dict[str, tuple[str, ...]] = {}
    children_of: dict[str, list[str]] = {}

    adults = [p for p in persons if not _is_student(persona_for_person, p)]
    if not adults:
        adults = list(persons)

    for household in households:
        adult_candidates = [
            p for p in household if not _is_student(persona_for_person, p)
        ]

        if len(adult_candidates) >= 2 and float(gen.random()) < float(
            cfg.spouse_pair_p
        ):
            a_idx = int(cast(int | np.integer, gen.integers(0, len(adult_candidates))))
            b_idx = int(
                cast(int | np.integer, gen.integers(0, len(adult_candidates) - 1))
            )
            if b_idx >= a_idx:
                b_idx += 1

            a = adult_candidates[a_idx]
            b = adult_candidates[b_idx]
            spouse_of[a] = b
            spouse_of[b] = a

        students = [p for p in household if _is_student(persona_for_person, p)]
        if not students:
            continue

        for child in students:
            if float(gen.random()) >= float(cfg.student_dependent_p):
                continue

            use_household_parents = (len(adult_candidates) >= 1) and (
                float(gen.random()) < float(cfg.co_reside_student_p)
            )
            parent_pool = adult_candidates if use_household_parents else adults
            if not parent_pool:
                continue

            want_two = (len(parent_pool) >= 2) and (
                float(gen.random()) < float(cfg.two_parent_p)
            )

            if want_two:
                p1 = parent_pool[
                    int(cast(int | np.integer, gen.integers(0, len(parent_pool))))
                ]
                p2 = p1

                tries = 0
                while p2 == p1 and tries < 8:
                    p2 = parent_pool[
                        int(cast(int | np.integer, gen.integers(0, len(parent_pool))))
                    ]
                    tries += 1

                if p2 == p1 and len(parent_pool) >= 2:
                    idx = parent_pool.index(p1)
                    p2 = parent_pool[(idx + 1) % len(parent_pool)]

                parents = (p1, p2) if p1 != p2 else (p1,)
            else:
                p1 = parent_pool[
                    int(cast(int | np.integer, gen.integers(0, len(parent_pool))))
                ]
                parents = (p1,)

            parents_of[child] = parents
            for parent in parents:
                children_of.setdefault(parent, []).append(child)

    adult_parents_of: dict[str, tuple[str, ...]] = {}
    adult_children_of: dict[str, list[str]] = {}

    global_supporters = [
        p for p in persons if _is_adult_supporter(persona_for_person, p)
    ]
    retirees = [p for p in persons if _is_retired(persona_for_person, p)]

    for parent in retirees:
        spouse = spouse_of.get(parent)
        if spouse is not None and _is_retired(persona_for_person, spouse):
            shared = adult_children_of.get(spouse)
            if shared is not None and float(gen.random()) < 0.85:
                adult_children_of[parent] = list(shared)
                for child in shared:
                    prev = adult_parents_of.get(child, ())
                    adult_parents_of[child] = prev + (parent,)
                continue

        if float(gen.random()) >= float(cfg.retiree_has_adult_child_p):
            continue

        household = households[household_id_for_person[parent]]
        resident_supporters = [
            p
            for p in household
            if p != parent and _is_adult_supporter(persona_for_person, p)
        ]

        use_resident = bool(resident_supporters) and (
            float(gen.random()) < float(cfg.retiree_support_co_reside_p)
        )
        pool = resident_supporters if use_resident else global_supporters

        chosen = _choose_unique_people(
            gen,
            pool,
            k=_sample_support_children_n(gen),
            exclude={parent},
        )
        if not chosen:
            continue

        adult_children_of[parent] = list(chosen)
        for child in chosen:
            prev = adult_parents_of.get(child, ())
            adult_parents_of[child] = prev + (parent,)

    _ = rng.float()

    return FamilyData(
        households=households,
        household_id_for_person=household_id_for_person,
        spouse_of=spouse_of,
        parents_of=parents_of,
        children_of=children_of,
        adult_parents_of=adult_parents_of,
        adult_children_of=adult_children_of,
    )
