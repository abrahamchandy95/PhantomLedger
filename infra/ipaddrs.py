from dataclasses import dataclass
from datetime import datetime, timedelta

from common.config import FraudConfig, WindowConfig
from common.ids import rand_ipv4
from common.rng import Rng
from common.timeutil import dt_str
from emit.tg_csv import CsvCell, CsvRow

from entities.people import PeopleData


@dataclass(frozen=True, slots=True)
class IpData:
    # ip_address -> blacklisted_ip (0/1)
    ips: dict[str, int]
    # HAS_IP rows: FROM, TO, first_seen, last_seen
    has_ip_rows: list[CsvRow]
    # person_id -> list of ip addresses (used later to stamp txns)
    person_ips: dict[str, list[str]]


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


def generate_ipaddrs(
    window: WindowConfig,
    fraud_cfg: FraudConfig,
    rng: Rng,
    people: PeopleData,
) -> IpData:
    """
    Generate ipaddress vertices and HAS_IP edges.

    - Baseline: each person gets 1–3 IPs, not blacklisted (blacklisted_ip=0).
    - Fraud rings: each ring gets one shared IP marked blacklisted (blacklisted_ip=1),
      linked to all ring participants across the full window.
    """
    start_date = window.start_date()
    days = int(window.days)

    ip_blacklist_flag: dict[str, int] = {}
    has_ip_rows: list[CsvRow] = []
    person_ips: dict[str, list[str]] = {p: [] for p in people.people}

    # Baseline IPs per person
    for person_id in people.people:
        n_ip = 1 + (1 if rng.coin(0.35) else 0) + (1 if rng.coin(0.10) else 0)

        for _ in range(n_ip):
            ip = rand_ipv4(rng.gen)

            if ip not in ip_blacklist_flag:
                ip_blacklist_flag[ip] = 0

            person_ips[person_id].append(ip)

            fs, ls = _sample_seen_window(rng, start_date, days)
            ip_row: list[CsvCell] = [person_id, ip, dt_str(fs), dt_str(ls)]
            has_ip_rows.append(ip_row)

    # Fraud ring shared IPs (blacklisted)
    if fraud_cfg.fraud_rings > 0:
        full_first = dt_str(start_date)
        full_last = dt_str(start_date + timedelta(days=days - 1))

        for ring in people.rings:
            shared_ip = rand_ipv4(rng.gen)
            ip_blacklist_flag[shared_ip] = 1

            for person_id in ring.ring_people:
                person_ips[person_id].append(shared_ip)

                shared_ip_row: list[CsvCell] = [
                    person_id,
                    shared_ip,
                    full_first,
                    full_last,
                ]
                has_ip_rows.append(shared_ip_row)

    return IpData(
        ips=ip_blacklist_flag,
        has_ip_rows=has_ip_rows,
        person_ips=person_ips,
    )


def build_ip_rows(data: IpData) -> list[CsvRow]:
    """
    Build rows for ipaddress.csv:
      ip_address, blacklisted_ip
    """
    rows: list[CsvRow] = []
    for ip in sorted(data.ips.keys()):
        row: list[CsvCell] = [ip, int(data.ips[ip])]
        rows.append(row)
    return rows
