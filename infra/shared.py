"""
Shared fraud-ring infrastructure state.

Maps ring_id -> (shared_device_id, shared_ip).

During fraud transactions (ring_id >= 0), the TransactionFactory will use the
shared ring device/IP with high probability instead of the person's
normal infrastructure. This creates the device/IP clustering signal
that GraphSAGE needs to detect mule accounts.
"""

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class SharedInfra:
    ring_device: dict[int, str]
    ring_ip: dict[int, str]

    use_shared_device_p: float = 0.85
    use_shared_ip_p: float = 0.80
