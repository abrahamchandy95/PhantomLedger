from dataclasses import dataclass

from common.config import FraudConfig, PopulationConfig
from common.ids import iter_person_ids
from common.rng import Rng


@dataclass(frozen=True, slots=True)
class RingPeople:
    ring_id: int
    ring_people: list[str]
    fraud_people: list[str]
    mule_people: list[str]
    victim_people: list[str]


@dataclass(frozen=True, slots=True)
class PeopleData:
    people: list[str]
    fraud_people: set[str]
    mule_people: set[str]
    victim_people: set[str]
    rings: list[RingPeople]


def generate_people(
    pop: PopulationConfig,
    fraud_cfg: FraudConfig,
    rng: Rng,
) -> PeopleData:
    """
    Generate all person IDs plus fraud/mule/victim flags.

    Design notes:
      - Ring participants are sampled without replacement across rings
        (i.e., a person is in at most one fraud ring).
      - Victims are sampled from outside all ring participants. A victim can
        appear in multiple rings (matches the spirit of your original code).
    """
    people = list(iter_person_ids(pop.persons))

    if fraud_cfg.fraud_rings <= 0:
        return PeopleData(
            people=people,
            fraud_people=set(),
            mule_people=set(),
            victim_people=set(),
            rings=[],
        )

    ring_total = fraud_cfg.fraud_rings * fraud_cfg.ring_size
    if ring_total >= len(people):
        raise ValueError("fraud ring participants exceed/consume the population size")

    # Sample all ring participants at once to avoid overlaps across rings.
    ring_pool = rng.choice_k(people, ring_total, replace=False)

    rings: list[RingPeople] = []
    fraud_people: set[str] = set()
    mule_people: set[str] = set()
    victim_people: set[str] = set()

    ring_all_set = set(ring_pool)
    non_ring_people = [p for p in people if p not in ring_all_set]

    for rid in range(fraud_cfg.fraud_rings):
        start = rid * fraud_cfg.ring_size
        end = start + fraud_cfg.ring_size
        ring_people = ring_pool[start:end]

        # Pick mules; ensure at least one fraudster.
        mules_k = min(fraud_cfg.mules_per_ring, len(ring_people))
        mule_list = rng.choice_k(ring_people, mules_k, replace=False)
        mule_set = set(mule_list)

        fraud_list = [p for p in ring_people if p not in mule_set]
        if not fraud_list:
            moved = mule_list.pop()
            fraud_list.append(moved)

        victims_k = fraud_cfg.victims_per_ring
        victim_list = (
            rng.choice_k(non_ring_people, victims_k, replace=False)
            if victims_k > 0
            else []
        )

        fraud_people.update(fraud_list)
        mule_people.update(mule_list)
        victim_people.update(victim_list)

        rings.append(
            RingPeople(
                ring_id=rid,
                ring_people=ring_people,
                fraud_people=fraud_list,
                mule_people=mule_list,
                victim_people=victim_list,
            )
        )

    return PeopleData(
        people=people,
        fraud_people=fraud_people,
        mule_people=mule_people,
        victim_people=victim_people,
        rings=rings,
    )
