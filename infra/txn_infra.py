from dataclasses import dataclass

from common.config import InfraConfig
from common.random import Rng


@dataclass(slots=True)
class TxnInfraAssigner:
    """
    Picks (device_id, ip_address) for a transaction based on the src account's owner.

    - Uses a "sticky current device/ip" per person
    - Occasionally switches with probability infra_cfg.switch_prob
    """

    acct_owner: dict[str, str]
    person_devices: dict[str, list[str]]
    person_ips: dict[str, list[str]]

    switch_p: float

    current_device: dict[str, str]
    current_ip: dict[str, str]

    @classmethod
    def build(
        cls,
        infra_cfg: InfraConfig,
        rng: Rng,
        *,
        acct_owner: dict[str, str],
        person_devices: dict[str, list[str]],
        person_ips: dict[str, list[str]],
    ) -> "TxnInfraAssigner":
        switch_p = float(infra_cfg.switch_prob)

        current_device: dict[str, str] = {}
        for person_id, devs in person_devices.items():
            if devs:
                current_device[person_id] = devs[0]

        current_ip: dict[str, str] = {}
        for person_id, ips in person_ips.items():
            if ips:
                current_ip[person_id] = ips[0]

        # Touch RNG once (keeps reproducibility stable if you change build order)
        _ = rng.float()

        return cls(
            acct_owner=acct_owner,
            person_devices=person_devices,
            person_ips=person_ips,
            switch_p=switch_p,
            current_device=current_device,
            current_ip=current_ip,
        )

    def pick_for_src(self, rng: Rng, src_acct: str) -> tuple[str | None, str | None]:
        person_id = self.acct_owner.get(src_acct)
        if person_id is None:
            return None, None

        dev = self._pick_from_pool(
            rng, person_id, self.person_devices, self.current_device
        )
        ip = self._pick_from_pool(rng, person_id, self.person_ips, self.current_ip)
        return dev, ip

    def _pick_from_pool(
        self,
        rng: Rng,
        person_id: str,
        pool: dict[str, list[str]],
        current: dict[str, str],
    ) -> str | None:
        items = pool.get(person_id)
        if not items:
            return None

        cur = current.get(person_id)
        if cur is None:
            chosen = items[0]
            current[person_id] = chosen
            return chosen

        if len(items) > 1 and rng.coin(self.switch_p):
            cand = cur
            tries = 0
            while cand == cur and tries < 5:
                cand = rng.choice(items)
                tries += 1
            current[person_id] = cand
            return cand

        return cur
