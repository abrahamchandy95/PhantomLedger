from collections.abc import Iterator

from common.timeline import format_ts
from infra import models
from ..csv_io import Row


def device(data: models.Devices) -> Iterator[Row]:
    """
    device.csv rows:
      (device_id, device_type, flagged_device)
    """
    for device_id, (device_type, flagged) in data.registry.items():
        yield (device_id, device_type, int(flagged))


def has_used(data: models.Devices) -> Iterator[Row]:
    """
    HAS_USED.csv rows:
      (FROM, TO, first_seen, last_seen)
    """
    for rec in data.has_used:
        yield (
            rec.person_id,
            rec.device_id,
            format_ts(rec.first_seen),
            format_ts(rec.last_seen),
        )


def ip_address(data: models.Ips) -> Iterator[Row]:
    """
    ipaddress.csv rows:
      (ip_address, blacklisted_ip)
    """
    for ip, flagged in data.registry.items():
        yield (ip, int(flagged))


def has_ip(data: models.Ips) -> Iterator[Row]:
    """
    HAS_IP.csv rows:
      (FROM, TO, first_seen, last_seen)
    """
    for rec in data.has_ip:
        yield (
            rec.person_id,
            rec.ip_address,
            format_ts(rec.first_seen),
            format_ts(rec.last_seen),
        )
