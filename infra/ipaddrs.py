from common import config
from common.ids import random_ip
from common.random import Rng
from common.timeline import sample_span, sample_short_span

from entities.models import People
from infra.models import Ips, IpUsage, RingPlan
from infra._pool import swap_delete


def build(
    window_cfg: config.Window,
    patterns_cfg: config.Patterns,
    rng: Rng,
    people: People,
    ring_plans: dict[int, RingPlan],
) -> Ips:
    start_date = window_cfg.start_date
    days = int(window_cfg.days)

    registry: dict[str, int] = {}
    has_ip: list[IpUsage] = []
    by_person: dict[str, list[str]] = {p: [] for p in people.ids}
    ring_map: dict[int, str] = {}

    for person_id in people.ids:
        n_ip = 1 + int(rng.coin(0.35)) + int(rng.coin(0.10))

        for _ in range(n_ip):
            ip = random_ip(rng.gen)
            if ip not in registry:
                registry[ip] = 0

            by_person[person_id].append(ip)

            first_seen, last_seen = sample_span(rng, start_date, days)
            has_ip.append(
                IpUsage(
                    person_id=person_id,
                    ip_address=ip,
                    first_seen=first_seen,
                    last_seen=last_seen,
                )
            )

    legit_pool = [p for p in people.ids if p not in people.frauds]
    remaining_legit = list(legit_pool)
    remaining_index = {person_id: idx for idx, person_id in enumerate(remaining_legit)}

    noise_p = float(patterns_cfg.legit_ip_noise_p)

    for anchor in legit_pool:
        if anchor not in remaining_index:
            continue
        if not rng.coin(noise_p):
            continue

        swap_delete(remaining_legit, remaining_index, anchor)

        if not remaining_legit:
            break

        extra_count = min(len(remaining_legit), rng.int(1, 4))
        peers = rng.choice_k(remaining_legit, extra_count, replace=False)
        group = [anchor, *peers]

        shared_ip = random_ip(rng.gen)
        registry[shared_ip] = 0
        first_seen, last_seen = sample_short_span(rng, start_date, days)

        for person_id in group:
            by_person[person_id].append(shared_ip)
            has_ip.append(
                IpUsage(
                    person_id=person_id,
                    ip_address=shared_ip,
                    first_seen=first_seen,
                    last_seen=last_seen,
                )
            )

        for person_id in peers:
            if person_id in remaining_index:
                swap_delete(remaining_legit, remaining_index, person_id)

    for plan in ring_plans.values():
        shared_ip = random_ip(rng.gen)
        registry[shared_ip] = 1
        ring_map[plan.ring_id] = shared_ip

        for person_id in plan.shared_ip_members:
            by_person[person_id].append(shared_ip)
            has_ip.append(
                IpUsage(
                    person_id=person_id,
                    ip_address=shared_ip,
                    first_seen=plan.first_seen,
                    last_seen=plan.last_seen,
                )
            )

    return Ips(
        registry=registry,
        has_ip=has_ip,
        by_person=by_person,
        ring_map=ring_map,
    )
