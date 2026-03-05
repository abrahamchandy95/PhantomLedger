from dataclasses import dataclass
from datetime import datetime, timedelta

from common.config import FraudConfig, WindowConfig
from common.ids import device_id_for_person, shared_ring_device_id
from common.rng import Rng
from common.timeutil import dt_str
from emit.tg_csv import CsvCell, CsvRow

from entities.people import PeopleData


_DEVICE_TYPES: tuple[str, ...] = ("android", "ios", "web", "desktop")


@dataclass(frozen=True, slots=True)
class DevicesData:
    # device_id -> (device_type, flagged_device)
    devices: dict[str, tuple[str, int]]
    # HAS_USED rows: FROM, TO, first_seen, last_seen
    has_used_rows: list[CsvRow]
    # person_id -> list of device_ids (used later to stamp txns)
    person_devices: dict[str, list[str]]


def _sample_seen_window(
    rng: Rng, start_date: datetime, days: int
) -> tuple[datetime, datetime]:
    """
    Sample first_seen and last_seen within the simulation window.

    Mirrors original logic:
      fs = start + rand_day
      ls = fs + rand(0..remaining_days)
    """
    if days <= 0:
        raise ValueError("days must be > 0")

    fs = start_date + timedelta(days=rng.int(0, days))
    remaining = days - (fs - start_date).days
    span = max(1, remaining)
    ls = fs + timedelta(days=rng.int(0, span))
    return fs, ls


def generate_devices(
    window: WindowConfig,
    fraud_cfg: FraudConfig,
    rng: Rng,
    people: PeopleData,
) -> DevicesData:
    start_date = window.start_date()
    days = int(window.days)

    devices: dict[str, tuple[str, int]] = {}
    has_used_rows: list[CsvRow] = []
    person_devices: dict[str, list[str]] = {p: [] for p in people.people}

    for p in people.people:
        n_dev = 1 + (1 if rng.coin(0.20) else 0)

        for idx in range(1, n_dev + 1):
            did = device_id_for_person(p, idx)
            dtype = rng.choice(_DEVICE_TYPES)
            flagged = 0

            devices[did] = (dtype, flagged)
            person_devices[p].append(did)

            fs, ls = _sample_seen_window(rng, start_date, days)
            used_row: list[CsvCell] = [p, did, dt_str(fs), dt_str(ls)]
            has_used_rows.append(used_row)

    if fraud_cfg.fraud_rings > 0:
        full_first = dt_str(start_date)
        full_last = dt_str(start_date + timedelta(days=days - 1))

        for ring in people.rings:
            shared = shared_ring_device_id(ring.ring_id)
            dtype = rng.choice(_DEVICE_TYPES)
            devices[shared] = (dtype, 1)

            for p in ring.ring_people:
                # Link ring participants to shared device for the full window
                person_devices[p].append(shared)
                shared_used_row: list[CsvCell] = [p, shared, full_first, full_last]
                has_used_rows.append(shared_used_row)

    return DevicesData(
        devices=devices,
        has_used_rows=has_used_rows,
        person_devices=person_devices,
    )


def build_device_rows(data: DevicesData) -> list[CsvRow]:
    """
    Build rows for device.csv:
      device_id, device_type, flagged_device
    """
    rows: list[CsvRow] = []
    for device_id in sorted(data.devices.keys()):
        dtype, flagged = data.devices[device_id]
        row: list[CsvCell] = [device_id, dtype, int(flagged)]
        rows.append(row)
    return rows
