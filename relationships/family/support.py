from dataclasses import dataclass
from typing import cast

import numpy as np

from common import config
from common.persona_names import RETIRED, SALARIED, FREELANCER, HNW, SMALLBIZ


@dataclass(frozen=True, slots=True)
class SupportTies:
    """Intermediate state holding cross-household financial support ties."""

    supported_parents: dict[str, tuple[str, ...]]
    supporting_children: dict[str, list[str]]


def _is_retired(persona_map: dict[str, str], person_id: str) -> bool:
    return persona_map.get(person_id, SALARIED) == RETIRED


def _can_support(persona_map: dict[str, str], person_id: str) -> bool:
    """People with active income who can provide financial support."""
    return persona_map.get(person_id, "salaried") in {
        SALARIED,
        FREELANCER,
        SMALLBIZ,
        HNW,
    }


def _sample_supporter_count(gen: np.random.Generator) -> int:
    """
    Samples how many adult children contribute to a retiree's support.
    65% -> 1 child, 27% -> 2 children, 8% -> 3 children.
    """
    draw = float(gen.random())
    if draw < 0.65:
        return 1
    if draw < 0.92:
        return 2
    return 3


def build(
    cfg: config.Family,
    gen: np.random.Generator,
    *,
    households: list[list[str]],
    household_map: dict[str, int],
    spouses: dict[str, str],
    people: list[str],
    persona_map: dict[str, str],
) -> SupportTies:
    """
    Builds financial support ties between retired parents and their adult children.
    """
    supported_parents: dict[str, tuple[str, ...]] = {}
    supporting_children: dict[str, list[str]] = {}

    # Identify potential financial supporters vs. those needing support in one pass
    supporters = [p for p in people if _can_support(persona_map, p)]
    retirees = [p for p in people if _is_retired(persona_map, p)]

    for retired_parent in retirees:
        # 1. Spousal Sync: If a spouse is already supported, the couple shares the same supporters
        spouse = spouses.get(retired_parent)
        if spouse is not None and _is_retired(persona_map, spouse):
            shared_supporters = supporting_children.get(spouse)
            if shared_supporters is not None and float(gen.random()) < 0.85:
                supporting_children[retired_parent] = list(shared_supporters)
                for adult_child in shared_supporters:
                    existing = supported_parents.get(adult_child, ())
                    supported_parents[adult_child] = existing + (retired_parent,)
                continue

        # 2. Probability check: Does this retiree actually receive external support?
        if float(gen.random()) >= float(cfg.retiree_has_child_p):
            continue

        # 3. Determine the pool of potential supporters
        hh_idx = household_map.get(retired_parent)
        household = households[hh_idx] if hh_idx is not None else []

        resident_supporters = [
            p for p in household if p != retired_parent and _can_support(persona_map, p)
        ]

        # Prefer co-residing supporters if configured
        use_resident = bool(resident_supporters) and (
            float(gen.random()) < float(cfg.retiree_coresides_p)
        )

        pool = resident_supporters if use_resident else supporters
        eligible = [p for p in pool if p != retired_parent]

        if not eligible:
            continue

        # 4. Vectorized Selection of Supporters
        k = min(len(eligible), _sample_supporter_count(gen))
        if k <= 0:
            continue

        # Use .tolist() to perfectly bypass NumPy-to-Python type hierarchy conflicts
        indices = cast(
            list[int], gen.choice(len(eligible), size=k, replace=False).tolist()
        )
        chosen_supporters = [eligible[i] for i in indices]

        # 5. Assign the support ties
        supporting_children[retired_parent] = chosen_supporters
        for adult_child in chosen_supporters:
            existing = supported_parents.get(adult_child, ())
            supported_parents[adult_child] = existing + (retired_parent,)

    return SupportTies(
        supported_parents=supported_parents,
        supporting_children=supporting_children,
    )
