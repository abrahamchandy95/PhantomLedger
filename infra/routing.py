from dataclasses import dataclass

from common import config
from common.random import Rng


@dataclass(slots=True)
class Router:
    """
    Routes a transaction to a specific (device_id, ip_address) based on the account owner.

    - Uses a "sticky current device/ip" per person.
    - Occasionally switches with probability infra_cfg.switch_prob.
    """

    owner_map: dict[str, str]
    devices_by_person: dict[str, list[str]]
    ips_by_person: dict[str, list[str]]

    switch_p: float

    current_device: dict[str, str]
    current_ip: dict[str, str]

    @classmethod
    def build(
        cls,
        infra_cfg: config.Infra,
        *,
        owner_map: dict[str, str],
        devices_by_person: dict[str, list[str]],
        ips_by_person: dict[str, list[str]],
    ) -> Router:
        switch_p = float(infra_cfg.switch_prob)

        current_device = {p: devs[0] for p, devs in devices_by_person.items() if devs}
        current_ip = {p: ips[0] for p, ips in ips_by_person.items() if ips}

        return cls(
            owner_map=owner_map,
            devices_by_person=devices_by_person,
            ips_by_person=ips_by_person,
            switch_p=switch_p,
            current_device=current_device,
            current_ip=current_ip,
        )

    def route_source(self, rng: Rng, src_acct: str) -> tuple[str | None, str | None]:
        """Looks up the owner and routes them to their current active infra."""
        person_id = self.owner_map.get(src_acct)
        if person_id is None:
            return None, None

        dev = self._route_from_pool(
            rng, person_id, self.devices_by_person, self.current_device
        )
        ip = self._route_from_pool(rng, person_id, self.ips_by_person, self.current_ip)
        return dev, ip

    def _route_from_pool(
        self,
        rng: Rng,
        person_id: str,
        pool: dict[str, list[str]],
        current: dict[str, str],
    ) -> str | None:
        """Determines if the user keeps their sticky infra or switches to an alternate."""
        items = pool.get(person_id)
        if not items:
            return None

        cur = current.get(person_id)
        if cur is None:
            chosen = items[0]
            current[person_id] = chosen
            return chosen

        if len(items) > 1 and rng.coin(self.switch_p):
            choices = [x for x in items if x != cur]
            cand = rng.choice(choices)

            current[person_id] = cand
            return cand

        return cur
