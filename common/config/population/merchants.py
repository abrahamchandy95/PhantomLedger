from dataclasses import dataclass, field

from common.validate import validate_metadata


@dataclass(frozen=True, slots=True)
class Merchants:
    # Repeatedly used, explicitly modeled merchants.
    per_10k_people: float = field(default=120.0, metadata={"ge": 0.0})
    long_tail_external_per_10k_people: float = field(
        default=400.0, metadata={"ge": 0.0}
    )
    long_tail_weight_share: float = field(
        default=0.18, metadata={"between": (0.0, 0.95)}
    )

    # Probability that a core merchant also banks at our institution.
    #
    # For a large retail bank (~10-12% deposit market share), geographic
    # co-location and commercial-banking bundling push the effective
    # overlap above the raw market share. NFIB 2023: 56% of small
    # business owners keep personal and business at the same bank.
    # National chains (Walmart, Amazon) bank at treasury/commercial
    # banks, pulling the blended rate back down.
    #
    # Net estimate: ~5-8% of a retail customer's merchant counterparties
    # also bank at the same large institution. 6% is the midpoint.
    in_bank_p: float = field(default=0.06, metadata={"between": (0.0, 1.0)})

    size_sigma: float = field(default=1.2, metadata={"gt": 0.0})
    long_tail_size_sigma: float = field(default=1.8, metadata={"gt": 0.0})

    favorite_min: int = field(default=8, metadata={"ge": 1})
    favorite_max: int = 30

    biller_min: int = field(default=2, metadata={"ge": 1})
    biller_max: int = 6

    explore_p: float = field(default=0.02, metadata={"between": (0.0, 1.0)})

    channel_merchant_p: float = field(default=0.82, metadata={"ge": 0.0})
    channel_bills_p: float = field(default=0.10, metadata={"ge": 0.0})
    channel_p2p_p: float = field(default=0.08, metadata={"ge": 0.0})

    txns_per_month: float = field(default=40.0, metadata={"ge": 0.0})

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
        validate_metadata(self)

        # Cross-field and list validations
        if self.favorite_max < self.favorite_min:
            raise ValueError(f"favorite_max must be >= {self.favorite_min}")

        if self.biller_max < self.biller_min:
            raise ValueError(f"biller_max must be >= {self.biller_min}")

        shares = (
            self.channel_merchant_p,
            self.channel_bills_p,
            self.channel_p2p_p,
        )
        if sum(shares) <= 0.0:
            raise ValueError("sum of channel probabilities must be > 0")

        if not self.categories:
            raise ValueError("categories must be non-empty")
