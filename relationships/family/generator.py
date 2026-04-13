import numpy as np

from common.config import population as pop_config
from common.random import derive_seed

from .links import build as build_internal_links
from .households import build as build_partition
from .network import FamilyGraph
from .support import build as build_support_ties


def build(
    households_cfg: pop_config.Households,
    dependents_cfg: pop_config.Dependents,
    retiree_support_cfg: pop_config.RetireeSupport,
    *,
    base_seed: int,
    people: list[str],
    persona_map: dict[str, str],
) -> FamilyGraph:
    """
    Orchestrates the generation of the familial relationship graph.
    """
    if not people:
        return FamilyGraph([], {}, {}, {}, {}, {}, {})

    gen = np.random.default_rng(derive_seed(base_seed, "family", "households"))

    partition = build_partition(
        households_cfg,
        gen,
        people=people,
    )

    internal_links = build_internal_links(
        households_cfg,
        dependents_cfg,
        gen,
        households=partition.households,
        people=people,
        persona_map=persona_map,
    )
    support_ties = build_support_ties(
        retiree_support_cfg,
        gen,
        households=partition.households,
        household_map=partition.household_map,
        spouses=internal_links.spouses,
        people=people,
        persona_map=persona_map,
    )

    return FamilyGraph(
        households=partition.households,
        household_map=partition.household_map,
        spouses=internal_links.spouses,
        parents=internal_links.parents,
        children=internal_links.children,
        supported_parents=support_ties.supported_parents,
        supporting_children=support_ties.supporting_children,
    )
