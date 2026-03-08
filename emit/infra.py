from collections.abc import Iterator

from common.temporal import dt_str
from emit.csv_io import CsvCell, CsvRow
from infra.devices import DevicesData
from infra.ipaddrs import IpData


def iter_device_rows(data: DevicesData) -> Iterator[CsvRow]:
    """
    device.csv rows:
      device_id, device_type, flagged_device
    """
    for device_id in sorted(data.devices):
        device_type, flagged = data.devices[device_id]
        row: list[CsvCell] = [device_id, device_type, int(flagged)]
        yield row


def iter_has_used_rows(data: DevicesData) -> Iterator[CsvRow]:
    """
    HAS_USED.csv rows:
      FROM, TO, first_seen, last_seen
    """
    for rec in data.has_used:
        row: list[CsvCell] = [
            rec.person_id,
            rec.device_id,
            dt_str(rec.first_seen),
            dt_str(rec.last_seen),
        ]
        yield row


def iter_ip_rows(data: IpData) -> Iterator[CsvRow]:
    """
    ipaddress.csv rows:
      ip_address, blacklisted_ip
    """
    for ip in sorted(data.ips):
        row: list[CsvCell] = [ip, int(data.ips[ip])]
        yield row


def iter_has_ip_rows(data: IpData) -> Iterator[CsvRow]:
    """
    HAS_IP.csv rows:
      FROM, TO, first_seen, last_seen
    """
    for rec in data.has_ip:
        row: list[CsvCell] = [
            rec.person_id,
            rec.ip_address,
            dt_str(rec.first_seen),
            dt_str(rec.last_seen),
        ]
        yield row
