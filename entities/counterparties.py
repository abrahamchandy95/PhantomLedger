"""
Counterparty pool builder.

Generates the external (and now partially internal) counterparty pools
that downstream transfer generators draw from when assigning:
  - employer payroll ACH credits
  - freelance client invoice payments
  - platform/gig payouts
  - processor settlement batches
  - business operating account draws
  - brokerage transfers

Pool sizes scale linearly with `population.size` via per-10k-people
densities defined in `common.config.population.counterparties`.

Employers and clients now support an `in_bank_p` parameter: the fraction
of the pool that also banks at our institution, producing internal (on-us)
counterparties alongside the traditional external ones.
"""

from collections.abc import Callable
from dataclasses import dataclass, field

from common import config
from common.ids import (
    brokerage_external_id,
    business_external_id,
    client_external_id,
    client_id,
    employer_external_id,
    employer_id,
    platform_external_id,
    processor_external_id,
)
from common.random import Rng

type IdFactory = Callable[[int], str]


def _scale_pool(per_10k: float, population: int, floor: int) -> int:
    """Scale a pool size linearly with population, subject to a floor."""
    scaled = round(per_10k * (population / 10_000.0))
    return max(floor, scaled)


@dataclass(frozen=True, slots=True)
class Pools:
    """
    All counterparty pools for a simulation world.

    Employers and clients are split into internal (on-us) and external
    (off-us) sublists. Other categories remain fully external since
    platforms, processors, and brokerages are institutional entities
    that bank at treasury/commercial banks rather than retail.
    """

    employer_internal_ids: list[str]
    employer_external_ids: list[str]
    employer_ids: list[str]

    client_internal_ids: list[str]
    client_external_ids: list[str]
    client_payer_ids: list[str]

    platform_ids: list[str]
    processor_ids: list[str]
    owner_business_ids: list[str]
    brokerage_ids: list[str]

    all_externals: list[str]
    all_internals: list[str] = field(default_factory=list)


def _split_pool(
    total: int,
    in_bank_p: float,
    internal_factory: IdFactory,
    external_factory: IdFactory,
    rng: Rng,
) -> tuple[list[str], list[str], list[str]]:
    """
    Generate a pool of `total` IDs, splitting each into internal or
    external based on `in_bank_p`. Returns (internals, externals, combined).

    Uses separate counters for internal and external to avoid ID collisions
    between the two prefix families.
    """
    internals: list[str] = []
    externals: list[str] = []
    combined: list[str] = []
    int_counter = 0
    ext_counter = 0

    for _ in range(total):
        acct_id: str
        if rng.coin(in_bank_p):
            int_counter += 1
            acct_id = internal_factory(int_counter)
            internals.append(acct_id)
        else:
            ext_counter += 1
            acct_id = external_factory(ext_counter)
            externals.append(acct_id)
        combined.append(acct_id)

    return internals, externals, combined


def build_pools(
    cfg: config.Counterparties,
    pop_cfg: config.Population,
    rng: Rng,
) -> Pools:
    """
    Build all counterparty pools scaled to population size.

    Employers and clients are split into internal/external based on
    `cfg.employer_in_bank_p` and `cfg.client_in_bank_p`. All other
    pools remain fully external.
    """
    pop = int(pop_cfg.size)

    # --- Employers: split by in_bank_p ---
    n_employers = _scale_pool(cfg.employers_per_10k, pop, 5)
    emp_int, emp_ext, emp_all = _split_pool(
        n_employers,
        float(cfg.employer_in_bank_p),
        employer_id,
        employer_external_id,
        rng,
    )

    # --- Clients: split by in_bank_p ---
    n_clients = _scale_pool(cfg.client_payers_per_10k, pop, 25)
    cli_int, cli_ext, cli_all = _split_pool(
        n_clients,
        float(cfg.client_in_bank_p),
        client_id,
        client_external_id,
        rng,
    )

    # --- Fully external pools ---
    n_platforms = _scale_pool(cfg.platforms_per_10k, pop, 2)
    platform_ids = [platform_external_id(i) for i in range(1, n_platforms + 1)]

    n_processors = _scale_pool(cfg.processors_per_10k, pop, 2)
    processor_ids = [processor_external_id(i) for i in range(1, n_processors + 1)]

    n_biz = _scale_pool(cfg.owner_businesses_per_10k, pop, 25)
    biz_ids = [business_external_id(i) for i in range(1, n_biz + 1)]

    n_brokerages = _scale_pool(cfg.brokerages_per_10k, pop, 5)
    brokerage_ids_list = [brokerage_external_id(i) for i in range(1, n_brokerages + 1)]

    # Aggregate external and internal lists for merge convenience.
    all_externals = (
        emp_ext + cli_ext + platform_ids + processor_ids + biz_ids + brokerage_ids_list
    )
    all_internals = emp_int + cli_int

    return Pools(
        employer_internal_ids=emp_int,
        employer_external_ids=emp_ext,
        employer_ids=emp_all,
        client_internal_ids=cli_int,
        client_external_ids=cli_ext,
        client_payer_ids=cli_all,
        platform_ids=platform_ids,
        processor_ids=processor_ids,
        owner_business_ids=biz_ids,
        brokerage_ids=brokerage_ids_list,
        all_externals=all_externals,
        all_internals=all_internals,
    )
