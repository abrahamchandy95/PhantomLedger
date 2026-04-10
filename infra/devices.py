from common import config
from common.ids import device_id, fraud_device_id
from common.random import Rng
from common.timeline import sample_span, sample_short_span

from entities.models import People
from infra._pool import swap_delete
from infra.models import Devices, DeviceUsage, RingPlan

_DEVICE_TYPES: tuple[str, ...] = ("android", "ios", "web", "desktop")


def build(
    window_cfg: config.Window,
    patterns_cfg: config.Patterns,
    rng: Rng,
    people: People,
    ring_plans: dict[int, RingPlan],
) -> Devices:
    registry: dict[str, tuple[str, int]] = {}
    has_used: list[DeviceUsage] = []
    by_person: dict[str, list[str]] = {person_id: [] for person_id in people.ids}
    ring_map: dict[int, str] = {}

    start = window_cfg.start_date
    days = int(window_cfg.days)

    for person_id in people.ids:
        n_dev = 2 if rng.coin(0.20) else 1

        for idx in range(1, n_dev + 1):
            dev_id = device_id(person_id, idx)
            device_type = rng.choice(_DEVICE_TYPES)

            registry[dev_id] = (device_type, 0)
            by_person[person_id].append(dev_id)

            first_seen, last_seen = sample_span(rng, start, days)
            has_used.append(
                DeviceUsage(
                    person_id=person_id,
                    device_id=dev_id,
                    first_seen=first_seen,
                    last_seen=last_seen,
                )
            )

    legit_pool = [
        person_id for person_id in people.ids if person_id not in people.frauds
    ]
    remaining_legit = list(legit_pool)
    remaining_index = {person_id: idx for idx, person_id in enumerate(remaining_legit)}

    legit_shared_counter = 1
    noise_p = float(patterns_cfg.legit_device_noise_p)

    for anchor in legit_pool:
        if anchor not in remaining_index:
            continue

        if not rng.coin(noise_p):
            continue

        swap_delete(remaining_legit, remaining_index, anchor)

        if not remaining_legit:
            break

        extra_count = min(len(remaining_legit), rng.int(1, 3))
        peers = rng.choice_k(remaining_legit, extra_count, replace=False)
        group = [anchor, *peers]

        shared_id = f"LD{legit_shared_counter:06d}"
        legit_shared_counter += 1

        registry[shared_id] = (rng.choice(_DEVICE_TYPES), 0)
        first_seen, last_seen = sample_short_span(rng, start, days)

        for person_id in group:
            by_person[person_id].append(shared_id)
            has_used.append(
                DeviceUsage(
                    person_id=person_id,
                    device_id=shared_id,
                    first_seen=first_seen,
                    last_seen=last_seen,
                )
            )

        for person_id in peers:
            if person_id in remaining_index:
                swap_delete(remaining_legit, remaining_index, person_id)

    for plan in ring_plans.values():
        shared_id = fraud_device_id(plan.ring_id)
        registry[shared_id] = (rng.choice(_DEVICE_TYPES), 1)
        ring_map[plan.ring_id] = shared_id

        for person_id in plan.shared_device_members:
            by_person[person_id].append(shared_id)
            has_used.append(
                DeviceUsage(
                    person_id=person_id,
                    device_id=shared_id,
                    first_seen=plan.first_seen,
                    last_seen=plan.last_seen,
                )
            )

    return Devices(
        registry=registry,
        has_used=has_used,
        by_person=by_person,
        ring_map=ring_map,
    )
