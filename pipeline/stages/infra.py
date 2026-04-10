from common import config
from common.random import Rng

from infra.devices import build as build_devices
from infra.ipaddrs import build as build_ips
from infra.ring_infra import build as build_ring_plans
from infra.routing import Router
from infra.shared import SharedInfra

from pipeline.state import Entities, Infra


def build(
    cfg: config.World,
    rng: Rng,
    entities: Entities,
) -> Infra:
    ring_plans = build_ring_plans(
        cfg.window,
        cfg.patterns,
        rng,
        entities.people.rings,
    )

    devices = build_devices(
        cfg.window,
        cfg.patterns,
        rng,
        entities.people,
        ring_plans,
    )

    ips = build_ips(
        cfg.window,
        cfg.patterns,
        rng,
        entities.people,
        ring_plans,
    )

    router = Router.build(
        cfg.infra,
        owner_map=entities.accounts.owner_map,
        devices_by_person=devices.by_person,
        ips_by_person=ips.by_person,
    )

    ring_infra = SharedInfra(
        ring_device=dict(devices.ring_map),
        ring_ip=dict(ips.ring_map),
    )

    return Infra(
        devices=devices,
        ips=ips,
        router=router,
        ring_infra=ring_infra,
    )
