from dataclasses import dataclass

from common.validate import between, ge, gt


@dataclass(frozen=True, slots=True)
class Merchants:
    per_10k_people: float = 12.0

    in_bank_p: float = 0.12
    size_sigma: float = 1.2

    favorite_min: int = 8
    favorite_max: int = 30

    biller_min: int = 2
    biller_max: int = 6

    explore_p: float = 0.02

    # Active channel mix for non-unknown day-to-day outflows only.
    # Generic external outflows are controlled by Events.unknown_outflow_p.
    channel_merchant_p: float = 0.68
    channel_bills_p: float = 0.20
    channel_p2p_p: float = 0.10

    txns_per_month: float = 18.0

    categories: tuple[str, ...] = (
        "grocery",
        "fuel",
        "utilities",
        "telecom",
        "ecommerce",
        "restaurant",
        "pharmacy",
        "retail_other",
        "insurance",
        "education",
    )

    def __post_init__(self) -> None:
        ge("per_10k_people", self.per_10k_people, 0.0)
        between("in_bank_p", self.in_bank_p, 0.0, 1.0)
        gt("size_sigma", self.size_sigma, 0.0)

        ge("favorite_min", self.favorite_min, 1)
        ge("favorite_max", self.favorite_max, self.favorite_min)
        ge("biller_min", self.biller_min, 1)
        ge("biller_max", self.biller_max, self.biller_min)

        between("explore_p", self.explore_p, 0.0, 1.0)

        shares = (
            self.channel_merchant_p,
            self.channel_bills_p,
            self.channel_p2p_p,
        )
        if any(s < 0.0 for s in shares):
            raise ValueError("channel probabilities must be >= 0")
        if sum(shares) <= 0.0:
            raise ValueError("sum of channel probabilities must be > 0")

        ge("txns_per_month", self.txns_per_month, 0.0)

        if not self.categories:
            raise ValueError("categories must be non-empty")
