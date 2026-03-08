from dataclasses import dataclass

from common.validate import (
    require_float_between,
    require_float_ge,
    require_float_gt,
    require_int_ge,
)


@dataclass(frozen=True, slots=True)
class MerchantsConfig:
    # Merchant count scales with population (keeps things stable when you change persons)
    merchants_per_10k_people: float = 12.0  # 0.12% of population as merchants

    # What fraction of merchants bank with us (have internal M... accounts)?
    in_bank_merchant_frac: float = 0.12

    # Merchant "size" (traffic) heavy-tail via lognormal
    size_lognormal_sigma: float = 1.2

    # Stickiness: for each payer, how many regular merchants/billers
    fav_merchants_min: int = 8
    fav_merchants_max: int = 30
    billers_min: int = 2
    billers_max: int = 6

    # Exploration probability per merchant payment (low => sticky)
    explore_p: float = 0.02

    # Channel mix for bank-visible outgoing payments.
    # external_unknown is retained here for backward compatibility, but the
    # active day-to-day channel mix now uses EventsConfig.unknown_outflow_p.
    share_merchant: float = 0.68
    share_bills: float = 0.20
    share_p2p: float = 0.10
    share_external_unknown: float = 0.02

    # Bank-visible payment intensity (NOT all payments overall; choose a subset)
    target_payments_per_person_per_month: float = 18.0

    def validate(self) -> None:
        require_float_ge(
            "merchants_per_10k_people",
            self.merchants_per_10k_people,
            0.0,
        )
        require_float_between(
            "in_bank_merchant_frac",
            self.in_bank_merchant_frac,
            0.0,
            1.0,
        )
        require_float_gt("size_lognormal_sigma", self.size_lognormal_sigma, 0.0)

        require_int_ge("fav_merchants_min", self.fav_merchants_min, 1)
        require_int_ge(
            "fav_merchants_max",
            self.fav_merchants_max,
            self.fav_merchants_min,
        )

        require_int_ge("billers_min", self.billers_min, 1)
        require_int_ge("billers_max", self.billers_max, self.billers_min)

        require_float_between("explore_p", self.explore_p, 0.0, 1.0)

        shares = (
            float(self.share_merchant),
            float(self.share_bills),
            float(self.share_p2p),
            float(self.share_external_unknown),
        )
        if any(s < 0.0 for s in shares):
            raise ValueError("merchant/bill/p2p/external shares must be >= 0")
        if float(sum(shares)) <= 0.0:
            raise ValueError("sum of shares must be > 0")

        require_float_ge(
            "target_payments_per_person_per_month",
            self.target_payments_per_person_per_month,
            0.0,
        )
