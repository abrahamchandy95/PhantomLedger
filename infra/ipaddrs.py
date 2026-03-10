from dataclasses import dataclass
from datetime import datetime, timedelta

from common.config import FraudConfig, WindowConfig
from common.ids import rand_ipv4
from common.random import Rng
from common.temporal import sample_seen_window
from entities.people import PeopleData


@dataclass(frozen=True, slots=True)
class IpUsage:
    person_id: str
    ip_address: str
    first_seen: datetime
    last_seen: datetime


@dataclass(frozen=True, slots=True)
class IpData:
    # ip_address -> blacklisted_ip (0/1)
    ips: dict[str, int]
    # domain records, not CSV rows
    has_ip: list[IpUsage]
    # person_id -> list of ip addresses (used later to stamp txns)
    person_ips: dict[str, list[str]]


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
    has_ip: list[IpUsage] = []
    person_ips: dict[str, list[str]] = {person_id: [] for person_id in people.people}

    # Baseline IPs per person
    for person_id in people.people:
        n_ip = 1 + (1 if rng.coin(0.35) else 0) + (1 if rng.coin(0.10) else 0)

        for _ in range(n_ip):
            ip = rand_ipv4(rng.gen)

            if ip not in ip_blacklist_flag:
                ip_blacklist_flag[ip] = 0

            person_ips[person_id].append(ip)

            first_seen, last_seen = sample_seen_window(rng, start_date, days)
            has_ip.append(
                IpUsage(
                    person_id=person_id,
                    ip_address=ip,
                    first_seen=first_seen,
                    last_seen=last_seen,
                )
            )

    # Fraud ring shared IPs (blacklisted)
    if fraud_cfg.fraud_rings > 0:
        full_first = start_date
        full_last = start_date + timedelta(days=days - 1)

        for ring in people.rings:
            shared_ip = rand_ipv4(rng.gen)
            ip_blacklist_flag[shared_ip] = 1

            for person_id in ring.ring_people:
                person_ips[person_id].append(shared_ip)
                has_ip.append(
                    IpUsage(
                        person_id=person_id,
                        ip_address=shared_ip,
                        first_seen=full_first,
                        last_seen=full_last,
                    )
                )

    return IpData(
        ips=ip_blacklist_flag,
        has_ip=has_ip,
        person_ips=person_ips,
    )
