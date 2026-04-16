"""
Rent payment channel routing.

Given a landlord type, pick a payment channel using
the per-type distribution in `common.config.population.landlords.Landlords`.

The router precomputes one CDF per type at construction time so the hot
path (one lookup per rent event) stays branch-free. Callers that don't
know the landlord type (fallback hub accounts, legacy plans) get the
generic `RENT` channel so the ledger stays consistent.
"""

from dataclasses import dataclass

from common.channels import RENT
from common.config.population.landlords import Landlords
from common.math import F64, build_cdf, cdf_pick
from common.random import Rng


@dataclass(frozen=True, slots=True)
class RentRouter:
    """Precomputed CDFs for each landlord type's channel distribution."""

    channel_names_by_type: dict[str, tuple[str, ...]]
    cdfs_by_type: dict[str, F64]
    default_channel: str = RENT

    @classmethod
    def from_config(cls, cfg: Landlords) -> "RentRouter":
        channel_names: dict[str, tuple[str, ...]] = {}
        cdfs: dict[str, F64] = {}

        for ltype, mix in cfg.channel_mix.items():
            # Preserve dict insertion order so the CDF and name tuple stay aligned.
            names = tuple(mix.keys())
            weights = [float(mix[name]) for name in names]
            channel_names[ltype] = names
            cdfs[ltype] = build_cdf(weights)

        return cls(
            channel_names_by_type=channel_names,
            cdfs_by_type=cdfs,
        )

    def pick_channel(self, rng: Rng, landlord_type: str | None) -> str:
        """
        Sample a channel for one rent event.

        `landlord_type` is allowed to be None because the rent generator
        falls back to hub accounts when no typed landlord pool exists. In
        that case we emit the generic RENT channel rather than guessing.
        """
        if landlord_type is None:
            return self.default_channel

        names = self.channel_names_by_type.get(landlord_type)
        if names is None:
            return self.default_channel

        cdf = self.cdfs_by_type[landlord_type]
        idx = cdf_pick(cdf, rng.float())
        return names[idx]
