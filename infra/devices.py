from dataclasses import dataclass
from datetime import datetime, timedelta

from common.config import FraudConfig, WindowConfig
from common.ids import device_id_for_person, shared_ring_device_id
from common.random import Rng
from common.temporal import sample_seen_window
from entities.people import PeopleData


_DEVICE_TYPES: tuple[str, ...] = ("android", "ios", "web", "desktop")


@dataclass(frozen=True, slots=True)
class DeviceUsage:
    person_id: str
    device_id: str
    first_seen: datetime
    last_seen: datetime


@dataclass(frozen=True, slots=True)
class DevicesData:
    # device_id -> (device_type, flagged_device)
    devices: dict[str, tuple[str, int]]
    # domain records, not CSV rows
    has_used: list[DeviceUsage]
    # person_id -> list of device_ids (used later to stamp txns)
    person_devices: dict[str, list[str]]


def generate_devices(
    window: WindowConfig,
    fraud_cfg: FraudConfig,
    rng: Rng,
    people: PeopleData,
) -> DevicesData:
    start_date = window.start_date()
    days = int(window.days)

    devices: dict[str, tuple[str, int]] = {}
    has_used: list[DeviceUsage] = []
    person_devices: dict[str, list[str]] = {
        person_id: [] for person_id in people.people
    }

    for person_id in people.people:
        n_dev = 1 + (1 if rng.coin(0.20) else 0)

        for idx in range(1, n_dev + 1):
            device_id = device_id_for_person(person_id, idx)
            device_type = rng.choice(_DEVICE_TYPES)
            flagged = 0

            devices[device_id] = (device_type, flagged)
            person_devices[person_id].append(device_id)

            first_seen, last_seen = sample_seen_window(rng, start_date, days)
            has_used.append(
                DeviceUsage(
                    person_id=person_id,
                    device_id=device_id,
                    first_seen=first_seen,
                    last_seen=last_seen,
                )
            )

    if fraud_cfg.fraud_rings > 0:
        full_first = start_date
        full_last = start_date + timedelta(days=days - 1)

        for ring in people.rings:
            shared = shared_ring_device_id(ring.ring_id)
            device_type = rng.choice(_DEVICE_TYPES)
            devices[shared] = (device_type, 1)

            for person_id in ring.ring_people:
                person_devices[person_id].append(shared)
                has_used.append(
                    DeviceUsage(
                        person_id=person_id,
                        device_id=shared,
                        first_seen=full_first,
                        last_seen=full_last,
                    )
                )

    return DevicesData(
        devices=devices,
        has_used=has_used,
        person_devices=person_devices,
    )
