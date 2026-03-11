from dataclasses import dataclass
from datetime import datetime
from typing import Protocol

from common.random import Rng
from common.transactions import Transaction


type InfraAttribution = tuple[str | None, str | None]
type Channel = str | None


class InfraPicker(Protocol):
    def pick_for_src(self, rng: Rng, src_acct: str) -> InfraAttribution: ...


@dataclass(frozen=True, slots=True, kw_only=True)
class TxnSpec:
    src: str
    dst: str
    amt: float
    ts: datetime
    is_fraud: int = 0
    ring_id: int = -1
    channel: Channel = None


@dataclass(frozen=True, slots=True)
class TxnFactory:
    rng: Rng
    infra: InfraPicker | None = None

    def make(self, spec: TxnSpec) -> Transaction:
        device_id: str | None = None
        ip_address: str | None = None
        if self.infra is not None:
            device_id, ip_address = self.infra.pick_for_src(self.rng, spec.src)

        return Transaction(
            source=spec.src,
            target=spec.dst,
            amount=float(spec.amt),
            timestamp=spec.ts,
            fraud_flag=int(spec.is_fraud),
            fraud_ring_id=int(spec.ring_id),
            device_id=device_id,
            ip_address=ip_address,
            channel=spec.channel,
        )
