from dataclasses import dataclass

from common.validate import (
    require_float_between,
    require_float_ge,
    require_float_gt,
    require_int_ge,
)


@dataclass(frozen=True, slots=True)
class MerchantsConfig:
    per_10k_people: float = 12.0

    in_bank_p: float = 0.12
    size_lognormal_sigma: float = 1.2

    fav_min: int = 8
    fav_max: int = 30

    billers_min: int = 2
    billers_max: int = 6

    explore_p: float = 0.02

    channel_merchant_p: float = 0.68
    channel_bills_p: float = 0.20
    channel_p2p_p: float = 0.10
    channel_external_unknown_p: float = 0.02

    target_payments_per_person_per_month: float = 18.0

    def validate(self) -> None:
        require_float_ge("per_10k_people", self.per_10k_people, 0.0)
        require_float_between("in_bank_p", self.in_bank_p, 0.0, 1.0)
        require_float_gt("size_lognormal_sigma", self.size_lognormal_sigma, 0.0)

        require_int_ge("fav_min", self.fav_min, 1)
        require_int_ge("fav_max", self.fav_max, self.fav_min)
        require_int_ge("billers_min", self.billers_min, 1)
        require_int_ge("billers_max", self.billers_max, self.billers_min)

        require_float_between("explore_p", self.explore_p, 0.0, 1.0)

        shares = (
            self.channel_merchant_p,
            self.channel_bills_p,
            self.channel_p2p_p,
            self.channel_external_unknown_p,
        )
        if any(s < 0.0 for s in shares):
            raise ValueError("channel probabilities must be >= 0")
        if sum(shares) <= 0.0:
            raise ValueError("sum of channel probabilities must be > 0")

        require_float_ge(
            "target_payments_per_person_per_month",
            self.target_payments_per_person_per_month,
            0.0,
        )
