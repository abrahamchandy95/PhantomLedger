"""
Generates external counterparty pools for recurring income and housing flows.

The original repo only needed employers and landlords. To make self-employed,
small-business, and HNW personas more realistic in bank data, we also build
pools for:
- client invoice payers (ACH credits)
- platforms / marketplaces
- merchant settlement processors
- business operating accounts used for owner draws
- brokerages / custodians

These remain synthetic external account IDs; they exist so transaction routing,
balance replay, and exports all recognize them as valid counterparties.
"""

from dataclasses import dataclass

from common.ids import (
    brokerage_external_id,
    business_external_id,
    client_external_id,
    employer_external_id,
    landlord_external_id,
    platform_external_id,
    processor_external_id,
)
from common.validate import ge


@dataclass(frozen=True, slots=True)
class PoolConfig:
    employers_per_10k: float = 25.0
    landlords_per_10k: float = 12.0

    # Large pools keep counterparty reuse plausible without having to model a
    # unique business entity for every freelancer or small business.
    client_payers_per_10k: float = 250.0
    owner_businesses_per_10k: float = 200.0
    brokerages_per_10k: float = 40.0

    # A much smaller number of shared platforms and processors is realistic.
    platforms_per_10k: float = 2.0
    processors_per_10k: float = 1.0

    def __post_init__(self) -> None:
        ge("employers_per_10k", self.employers_per_10k, 1.0)
        ge("landlords_per_10k", self.landlords_per_10k, 1.0)
        ge("client_payers_per_10k", self.client_payers_per_10k, 1.0)
        ge("owner_businesses_per_10k", self.owner_businesses_per_10k, 1.0)
        ge("brokerages_per_10k", self.brokerages_per_10k, 1.0)
        ge("platforms_per_10k", self.platforms_per_10k, 1.0)
        ge("processors_per_10k", self.processors_per_10k, 1.0)


DEFAULT_POOL_CONFIG = PoolConfig()


@dataclass(frozen=True, slots=True)
class Pools:
    employer_ids: list[str]
    landlord_ids: list[str]
    client_payer_ids: list[str]
    platform_ids: list[str]
    processor_ids: list[str]
    owner_business_ids: list[str]
    brokerage_ids: list[str]
    all_externals: list[str]


def build(
    population_size: int,
    cfg: PoolConfig = DEFAULT_POOL_CONFIG,
) -> Pools:
    n_employers = max(
        5, int(round(cfg.employers_per_10k * (population_size / 10_000.0)))
    )
    n_landlords = max(
        3, int(round(cfg.landlords_per_10k * (population_size / 10_000.0)))
    )
    n_clients = max(
        25, int(round(cfg.client_payers_per_10k * (population_size / 10_000.0)))
    )
    n_platforms = max(
        2, int(round(cfg.platforms_per_10k * (population_size / 10_000.0)))
    )
    n_processors = max(
        2, int(round(cfg.processors_per_10k * (population_size / 10_000.0)))
    )
    n_owner_businesses = max(
        25,
        int(round(cfg.owner_businesses_per_10k * (population_size / 10_000.0))),
    )
    n_brokerages = max(
        5, int(round(cfg.brokerages_per_10k * (population_size / 10_000.0)))
    )

    employer_ids = [employer_external_id(i) for i in range(1, n_employers + 1)]
    landlord_ids = [landlord_external_id(i) for i in range(1, n_landlords + 1)]
    client_payer_ids = [client_external_id(i) for i in range(1, n_clients + 1)]
    platform_ids = [platform_external_id(i) for i in range(1, n_platforms + 1)]
    processor_ids = [processor_external_id(i) for i in range(1, n_processors + 1)]
    owner_business_ids = [
        business_external_id(i) for i in range(1, n_owner_businesses + 1)
    ]
    brokerage_ids = [brokerage_external_id(i) for i in range(1, n_brokerages + 1)]

    all_externals = (
        employer_ids
        + landlord_ids
        + client_payer_ids
        + platform_ids
        + processor_ids
        + owner_business_ids
        + brokerage_ids
    )

    return Pools(
        employer_ids=employer_ids,
        landlord_ids=landlord_ids,
        client_payer_ids=client_payer_ids,
        platform_ids=platform_ids,
        processor_ids=processor_ids,
        owner_business_ids=owner_business_ids,
        brokerage_ids=brokerage_ids,
        all_externals=all_externals,
    )
