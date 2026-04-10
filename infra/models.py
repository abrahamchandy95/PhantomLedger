from dataclasses import dataclass
from datetime import datetime


@dataclass(frozen=True, slots=True)
class DeviceUsage:
    person_id: str
    device_id: str
    first_seen: datetime
    last_seen: datetime


@dataclass(frozen=True, slots=True)
class Devices:
    registry: dict[str, tuple[str, int]]
    has_used: list[DeviceUsage]
    by_person: dict[str, list[str]]
    ring_map: dict[int, str]


@dataclass(frozen=True, slots=True)
class IpUsage:
    person_id: str
    ip_address: str
    first_seen: datetime
    last_seen: datetime


@dataclass(frozen=True, slots=True)
class Ips:
    registry: dict[str, int]
    has_ip: list[IpUsage]
    by_person: dict[str, list[str]]
    ring_map: dict[int, str]


@dataclass(frozen=True, slots=True)
class RingPlan:
    ring_id: int
    first_seen: datetime
    last_seen: datetime
    shared_device_members: tuple[str, ...]
    shared_ip_members: tuple[str, ...]
