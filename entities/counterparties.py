"""
Generates external counterparty pools for employer and landlord accounts.

In US retail banking, salary deposits arrive via ACH from external
institutions in ~92% of cases (PayrollOrg 2024). Even same-bank
employers typically route payroll through third-party processors
(ADP, Paychex) whose originating bank differs from the employee's.

Census SUSB reports ~8M business establishments for ~330M people,
roughly 24 per 10,000. Employer-employee concentration follows a
power law: 89% of establishments have <20 employees, but firms
with 500+ account for ~51% of total employment.
"""

from dataclasses import dataclass

from common.ids import employer_external_id, landlord_external_id
from common.validate import ge


@dataclass(frozen=True, slots=True)
class PoolConfig:
    """
    Controls the size of employer and landlord pools.

    employers_per_10k:
        Distinct employer entities per 10,000 population.
        Census SUSB: ~24 establishments per 10K nationally.
        A bank sees a subset; 25 is a reasonable default.

    landlords_per_10k:
        Distinct landlord/property-mgmt entities per 10,000 population.
        ~34% of US households rent. Property management is consolidated,
        so fewer distinct landlord entities than employers.
    """

    employers_per_10k: float = 25.0
    landlords_per_10k: float = 12.0

    def __post_init__(self) -> None:
        ge("employers_per_10k", self.employers_per_10k, 1.0)
        ge("landlords_per_10k", self.landlords_per_10k, 1.0)


DEFAULT_POOL_CONFIG = PoolConfig()


@dataclass(frozen=True, slots=True)
class Pools:
    """Pre-generated pools of external employer and landlord accounts."""

    employer_ids: list[str]
    landlord_ids: list[str]
    all_externals: list[str]


def build(
    population_size: int,
    cfg: PoolConfig = DEFAULT_POOL_CONFIG,
) -> Pools:
    """
    Generate external account IDs for employers and landlords.

    Each type uses its own prefix (XE for employers, XL for landlords),
    so counters start at 1 independently with no collision risk
    regardless of population scale.
    """
    n_employers = max(
        5, int(round(cfg.employers_per_10k * (population_size / 10_000.0)))
    )
    n_landlords = max(
        3, int(round(cfg.landlords_per_10k * (population_size / 10_000.0)))
    )

    employer_ids = [employer_external_id(i) for i in range(1, n_employers + 1)]
    landlord_ids = [landlord_external_id(i) for i in range(1, n_landlords + 1)]

    return Pools(
        employer_ids=employer_ids,
        landlord_ids=landlord_ids,
        all_externals=employer_ids + landlord_ids,
    )
