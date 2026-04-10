from common import config
from common.config.simulation.fraud import FraudSampler
from common.ids import customers
from common.random import Rng

from .fraudsters import (
    assign_solo_fraudsters,
    build_rings,
    inject_multi_ring_mules,
)
from . import models


def _empty_people(people: list[str]) -> models.People:
    return models.People(
        ids=people,
        frauds=set(),
        mules=set(),
        victims=set(),
        solo_frauds=set(),
        rings=[],
    )


def generate(
    pop_cfg: config.Population,
    fraud_cfg: config.Fraud,
    rng: Rng,
) -> models.People:
    """
    Generate all person IDs plus fraud/mule/victim/solo flags.
    """
    people = list(customers(pop_cfg.size))

    if pop_cfg.size == 0:
        return _empty_people(people)

    sampler = FraudSampler(cfg=fraud_cfg, rng=rng)

    num_rings = sampler.ring_count(pop_cfg.size)
    if num_rings <= 0 and fraud_cfg.solos_per_10k <= 0.0:
        return _empty_people(people)

    ceiling = fraud_cfg.max_participants(pop_cfg.size)

    ring_sizes: list[int] = []
    ring_mule_counts: list[int] = []
    ring_victim_counts: list[int] = []
    consumed = 0

    for _ in range(num_rings):
        params = sampler.ring_params(
            remaining_budget=ceiling - consumed,
            eligible_victims=pop_cfg.size - consumed,
        )
        if params is None:
            break

        ring_sizes.append(params.ring_size)
        ring_mule_counts.append(params.mule_count)
        ring_victim_counts.append(params.victim_count)
        consumed += params.ring_size

    actual_num_rings = len(ring_sizes)
    total_ring_people = sum(ring_sizes)

    if actual_num_rings == 0 and fraud_cfg.solos_per_10k <= 0.0:
        return _empty_people(people)

    ring_all_set: set[str]

    if total_ring_people > 0:
        if total_ring_people >= len(people):
            raise ValueError(
                f"fraud ring participants ({total_ring_people}) exceed "
                + f"population ({len(people)})"
            )

        ring_pool = rng.choice_k(people, total_ring_people, replace=False)
        ring_all_set = set(ring_pool)
        non_ring_people = [p for p in people if p not in ring_all_set]
    else:
        ring_pool = []
        ring_all_set = set()
        non_ring_people = list(people)

    if actual_num_rings > 0:
        rings, fraud_people, mule_people, victim_people = build_rings(
            fraud_cfg,
            rng,
            ring_pool=ring_pool,
            non_ring_people=non_ring_people,
            ring_sizes=ring_sizes,
            ring_mule_counts=ring_mule_counts,
            ring_victim_counts=ring_victim_counts,
        )
    else:
        rings: list[models.Ring] = []
        fraud_people: set[str] = set()
        mule_people: set[str] = set()
        victim_people: set[str] = set()

    inject_multi_ring_mules(fraud_cfg, rng, rings, mule_people)

    remaining_budget = max(0, ceiling - total_ring_people)
    solo_fraudsters = assign_solo_fraudsters(
        sampler,
        people=people,
        ring_all_set=ring_all_set,
        fraud_people=fraud_people,
        mule_people=mule_people,
        remaining_budget=remaining_budget,
    )

    return models.People(
        ids=people,
        frauds=fraud_people,
        mules=mule_people,
        victims=victim_people,
        solo_frauds=solo_fraudsters,
        rings=rings,
    )
