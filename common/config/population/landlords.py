"""
Landlord pool configuration.

Holds:
  - how dense the landlord counterparty pool is per 10,000 people,
  - the unit-weighted type distribution (RHFS 2021 anchored),
  - the per-type payment channel mix used by rent_routing.

This is the single source of truth for "how many landlords should exist"
and "how does each landlord type collect rent". Downstream generators
never hard-code these numbers.
"""

from dataclasses import dataclass, field

from common.channels import RENT, RENT_ACH, RENT_CHECK, RENT_P2P, RENT_PORTAL
from common.landlord_types import (
    CORPORATE,
    DEFAULT_TYPE_SHARES,
    INDIVIDUAL,
    LLC_SMALL,
)
from common.validate import between, validate_metadata


def _default_channel_mix() -> dict[str, dict[str, float]]:
    """
    Per-type payment channel mix.

    Values are modeling judgments informed by the Baselane 2024 landlord
    survey (~half of landlords accept ACH), TurboTenant's report that
    ~65% of digital rent payments flow over ACH, and the well-documented
    pattern that individual landlords lean on Zelle/check while corporate
    property management uses portal-based ACH.

    Numbers sum to 1.0 per type and are validated at construction time.
    """
    return {
        INDIVIDUAL: {
            RENT_P2P: 0.40,  # Zelle / Venmo style
            RENT_CHECK: 0.25,  # paper check
            RENT_ACH: 0.25,  # direct ACH credit
            RENT: 0.10,  # generic / online bill pay
        },
        LLC_SMALL: {
            RENT_ACH: 0.55,
            RENT_CHECK: 0.30,
            RENT_P2P: 0.15,
        },
        CORPORATE: {
            RENT_PORTAL: 0.95,  # property-management portal ACH debit
            RENT_ACH: 0.05,
        },
    }


@dataclass(frozen=True, slots=True)
class Landlords:
    # Density. Roughly matches the legacy entities/counterparties.py default
    # of 12 landlords per 10k people. Increase for more counterparty variety.
    per_10k_people: float = field(default=12.0, metadata={"ge": 1.0})

    # Unit-weighted share. Keys must be from common.landlord_types.ALL.
    type_shares: dict[str, float] = field(
        default_factory=lambda: dict(DEFAULT_TYPE_SHARES)
    )

    # Per-type channel mix. Each inner dict must sum to 1.0.
    channel_mix: dict[str, dict[str, float]] = field(
        default_factory=_default_channel_mix
    )

    def __post_init__(self) -> None:
        validate_metadata(self)

        if not self.type_shares:
            raise ValueError("type_shares must be non-empty")

        total = sum(self.type_shares.values())
        if not 0.99 <= total <= 1.01:
            raise ValueError(f"type_shares must sum to ~1.0, got {total:.4f}")
        for name, share in self.type_shares.items():
            between(f"type_shares[{name}]", share, 0.0, 1.0)

        if not self.channel_mix:
            raise ValueError("channel_mix must be non-empty")

        for ltype, mix in self.channel_mix.items():
            if not mix:
                raise ValueError(f"channel_mix[{ltype}] must be non-empty")
            total_mix = sum(mix.values())
            if not 0.99 <= total_mix <= 1.01:
                raise ValueError(
                    f"channel_mix[{ltype}] must sum to ~1.0, got {total_mix:.4f}"
                )
            for channel, weight in mix.items():
                between(f"channel_mix[{ltype}][{channel}]", weight, 0.0, 1.0)

        # Every declared type needs a channel mix.
        missing = set(self.type_shares) - set(self.channel_mix)
        if missing:
            raise ValueError(
                f"channel_mix missing entries for types: {sorted(missing)}"
            )
