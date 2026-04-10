from dataclasses import dataclass
from datetime import datetime
from typing import Protocol

from common.random import Rng
from common.transactions import Transaction
from infra.shared import SharedInfra

type InfraAttribution = tuple[str | None, str | None]
type Channel = str | None


class InfraRouter(Protocol):
    def route_source(self, rng: Rng, src_acct: str) -> InfraAttribution: ...


@dataclass(frozen=True, slots=True, kw_only=True)
class TransactionDraft:
    """The raw properties needed to assemble a completed Transaction."""

    source: str
    destination: str
    amount: float
    timestamp: datetime
    is_fraud: int = 0
    ring_id: int = -1
    channel: Channel = None


@dataclass(frozen=True, slots=True)
class TransactionFactory:
    """Constructs final Transaction objects and attaches the correct routing infrastructure."""

    rng: Rng
    infra: InfraRouter | None = None
    ring_infra: SharedInfra | None = None

    def make(self, draft: TransactionDraft) -> Transaction:
        device_id: str | None = None
        ip_address: str | None = None

        if draft.ring_id >= 0 and self.ring_infra is not None:
            shared_device = self.ring_infra.ring_device.get(draft.ring_id)
            shared_ip = self.ring_infra.ring_ip.get(draft.ring_id)

            if shared_device is not None and self.rng.coin(
                self.ring_infra.use_shared_device_p
            ):
                device_id = shared_device

            if shared_ip is not None and self.rng.coin(self.ring_infra.use_shared_ip_p):
                ip_address = shared_ip

        if self.infra is not None and (device_id is None or ip_address is None):
            personal_device, personal_ip = self.infra.route_source(
                self.rng, draft.source
            )

            if device_id is None:
                device_id = personal_device
            if ip_address is None:
                ip_address = personal_ip

        return Transaction(
            source=draft.source,
            target=draft.destination,
            amount=float(draft.amount),
            timestamp=draft.timestamp,
            fraud_flag=int(draft.is_fraud),
            fraud_ring_id=int(draft.ring_id),
            device_id=device_id,
            ip_address=ip_address,
            channel=draft.channel,
        )
