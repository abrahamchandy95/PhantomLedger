import numpy as np

from common.config import FamilyConfig
from common.random import Rng, derived_seed

from .household_links import build_household_links
from .households import build_household_graph
from .models import FamilyData
from .support import build_support_links


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

    household_graph = build_household_graph(
        cfg,
        gen,
        persons=persons,
    )
    household_links = build_household_links(
        cfg,
        gen,
        households=household_graph.households,
        persons=persons,
        persona_for_person=persona_for_person,
    )
    support_links = build_support_links(
        cfg,
        gen,
        households=household_graph.households,
        household_id_for_person=household_graph.household_id_for_person,
        spouse_of=household_links.spouse_of,
        persons=persons,
        persona_for_person=persona_for_person,
    )

    _ = rng.float()

    return FamilyData(
        households=household_graph.households,
        household_id_for_person=household_graph.household_id_for_person,
        spouse_of=household_links.spouse_of,
        parents_of=household_links.parents_of,
        children_of=household_links.children_of,
        adult_parents_of=support_links.adult_parents_of,
        adult_children_of=support_links.adult_children_of,
    )
