from dataclasses import dataclass

from common.validate import (
    between,
    ge,
)


@dataclass(frozen=True, slots=True)
class Patterns:
    burst_days_min: int = 7
    burst_days_max: int = 14

    shared_device_p: float = 0.80
    shared_ip_p: float = 0.75
    legit_device_noise_p: float = 0.01
    legit_ip_noise_p: float = 0.01

    mule_inbound_min: int = 10
    mule_inbound_max: int = 25

    hops_min: int = 1
    hops_max: int = 3

    delay_hours_min: int = 1
    delay_hours_max: int = 24

    cashout_p: float = 0.85
    camouflage_p: float = 0.10

    amount_median: float = 1800.0
    amount_sigma: float = 0.90

    homophily_min: float = 0.10
    shared_device_rate_min: float = 0.60

    def __post_init__(self) -> None:
        ge("burst_days_min", self.burst_days_min, 1)
        ge("burst_days_max", self.burst_days_max, self.burst_days_min)

        between("shared_device_p", self.shared_device_p, 0.0, 1.0)
        between("shared_ip_p", self.shared_ip_p, 0.0, 1.0)
        between("legit_device_noise_p", self.legit_device_noise_p, 0.0, 1.0)
        between("legit_ip_noise_p", self.legit_ip_noise_p, 0.0, 1.0)

        ge("mule_inbound_min", self.mule_inbound_min, 1)
        ge("mule_inbound_max", self.mule_inbound_max, self.mule_inbound_min)

        ge("hops_min", self.hops_min, 0)
        ge("hops_max", self.hops_max, self.hops_min)

        ge("delay_hours_min", self.delay_hours_min, 0)
        ge("delay_hours_max", self.delay_hours_max, self.delay_hours_min)

        between("cashout_p", self.cashout_p, 0.0, 1.0)
        between("camouflage_p", self.camouflage_p, 0.0, 1.0)

        ge("amount_median", self.amount_median, 1.0)
        ge("amount_sigma", self.amount_sigma, 0.0)

        between("homophily_min", self.homophily_min, 0.0, 1.0)
        between("shared_device_rate_min", self.shared_device_rate_min, 0.0, 1.0)
