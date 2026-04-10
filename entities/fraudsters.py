from common import config
from common.config.simulation.fraud import FraudSampler
from common.random import Rng

from . import models


def build_rings(
    fraud_cfg: config.Fraud,
    rng: Rng,
    *,
    ring_pool: list[str],
    non_ring_people: list[str],
    ring_sizes: list[int],
    ring_mule_counts: list[int],
    ring_victim_counts: list[int],
) -> tuple[list[models.Ring], set[str], set[str], set[str]]:
    """
    Construct individual rings from pre-sampled parameters.

    Returns (rings, fraud_people, mule_people, victim_people).
    """
    rings: list[models.Ring] = []
    fraud_people: set[str] = set()
    mule_people: set[str] = set()
    victim_people: set[str] = set()

    all_victims_so_far: list[str] = []

    cursor = 0
    for rid in range(len(ring_sizes)):
        ring_size = ring_sizes[rid]
        num_mules = ring_mule_counts[rid]
        num_victims = ring_victim_counts[rid]

        ring_members = ring_pool[cursor : cursor + ring_size]
        cursor += ring_size

        mule_list = rng.choice_k(ring_members, num_mules, replace=False)
        mule_set = set(mule_list)
        fraud_list = [p for p in ring_members if p not in mule_set]

        if not fraud_list and mule_list:
            promoted = mule_list.pop()
            mule_set.discard(promoted)
            fraud_list.append(promoted)

        available_victims = min(num_victims, len(non_ring_people))
        if available_victims > 0:
            victim_list = rng.choice_k(
                non_ring_people, available_victims, replace=False
            )
        else:
            victim_list = []

        if all_victims_so_far:
            for i in range(len(victim_list)):
                if rng.coin(float(fraud_cfg.repeat_victim_p)):
                    victim_list[i] = rng.choice(all_victims_so_far)

        all_victims_so_far.extend(victim_list)

        fraud_people.update(fraud_list)
        mule_people.update(mule_set)
        victim_people.update(victim_list)

        rings.append(
            models.Ring(
                id=rid,
                members=ring_members,
                frauds=fraud_list,
                mules=list(mule_set),
                victims=victim_list,
            )
        )

    return rings, fraud_people, mule_people, victim_people


def inject_multi_ring_mules(
    fraud_cfg: config.Fraud,
    rng: Rng,
    rings: list[models.Ring],
    mule_people: set[str],
) -> None:
    """
    Some mules participate in multiple rings simultaneously.

    Merseyside OCG study: 63% of groups cooperated with at least
    one other group. This creates cross-community edges that break
    the "isolated cluster = fraud" assumption.

    Mutates ring mule_people lists in place.
    """
    if len(rings) < 2:
        return

    all_mules = list(mule_people)
    if not all_mules:
        return

    for mule_id in all_mules:
        if not rng.coin(float(fraud_cfg.multi_ring_mule_p)):
            continue

        home_ring_ids = {ring.id for ring in rings if mule_id in ring.mules}

        other_rings = [ring for ring in rings if ring.id not in home_ring_ids]
        if not other_rings:
            continue

        target_ring = rng.choice(other_rings)
        if mule_id not in target_ring.mules:
            target_ring.mules.append(mule_id)


def assign_solo_fraudsters(
    sampler: FraudSampler,
    *,
    people: list[str],
    ring_all_set: set[str],
    fraud_people: set[str],
    mule_people: set[str],
    remaining_budget: int,
) -> set[str]:
    """
    Solo fraudsters: individual bad actors with no ring.
    """
    solo_count = sampler.solo_count(len(people), remaining_budget)
    if solo_count <= 0:
        return set()

    already_assigned = ring_all_set | fraud_people | mule_people
    eligible = [p for p in people if p not in already_assigned]

    if not eligible:
        return set()

    solo_count = min(solo_count, len(eligible))
    solo_people = sampler.rng.choice_k(eligible, solo_count, replace=False)
    solo_set = set(solo_people)

    fraud_people.update(solo_set)
    return solo_set
