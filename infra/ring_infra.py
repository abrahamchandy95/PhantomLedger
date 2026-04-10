from datetime import datetime, timedelta

from common import config
from common.random import Rng

from entities.models import Ring
from infra.models import RingPlan


def _sample_window(
    window_cfg: config.Window,
    patterns_cfg: config.Patterns,
    rng: Rng,
) -> tuple[datetime, datetime]:
    start_date = window_cfg.start_date
    total_days = int(window_cfg.days)

    burst_min = max(1, int(patterns_cfg.burst_days_min))
    burst_max = max(burst_min, int(patterns_cfg.burst_days_max))
    burst_len = min(total_days, rng.int(burst_min, burst_max + 1))

    max_start_offset = max(0, total_days - burst_len)
    start_offset = 0 if max_start_offset == 0 else rng.int(0, max_start_offset + 1)

    first_seen = start_date + timedelta(days=start_offset)
    last_seen = first_seen + timedelta(days=burst_len - 1)
    return first_seen, last_seen


def _sample_members(
    rng: Rng,
    members: list[str],
    share_p: float,
) -> tuple[str, ...]:
    if not members:
        return ()

    selected = [p for p in members if rng.coin(share_p)]

    min_required = min(2, len(members))

    if len(selected) < min_required:
        selected = rng.choice_k(members, min_required, replace=False)

    return tuple(selected)


def build(
    window_cfg: config.Window,
    patterns_cfg: config.Patterns,
    rng: Rng,
    rings: list[Ring],
) -> dict[int, RingPlan]:
    plans: dict[int, RingPlan] = {}

    dev_p = float(patterns_cfg.shared_device_p)
    ip_p = float(patterns_cfg.shared_ip_p)

    for ring in rings:
        first_seen, last_seen = _sample_window(window_cfg, patterns_cfg, rng)

        plans[ring.id] = RingPlan(
            ring_id=ring.id,
            first_seen=first_seen,
            last_seen=last_seen,
            shared_device_members=_sample_members(rng, ring.members, dev_p),
            shared_ip_members=_sample_members(rng, ring.members, ip_p),
        )

    return plans
