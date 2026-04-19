"""
Counterparty pool density configuration.

Controls the number of external/internal counterparty accounts generated
per 10,000 population members. Each density is a modeling parameter that
can be tuned without touching the builder logic in
`entities/counterparties.py`.
"""

from dataclasses import dataclass, field

from common.validate import validate_metadata


@dataclass(frozen=True, slots=True)
class Counterparties:
    employers_per_10k: float = field(default=25.0, metadata={"ge": 0.0})
    client_payers_per_10k: float = field(default=250.0, metadata={"ge": 0.0})
    owner_businesses_per_10k: float = field(default=200.0, metadata={"ge": 0.0})
    brokerages_per_10k: float = field(default=40.0, metadata={"ge": 0.0})
    platforms_per_10k: float = field(default=2.0, metadata={"ge": 0.0})
    processors_per_10k: float = field(default=1.0, metadata={"ge": 0.0})

    # Probability that an employer also banks at our institution.
    #
    # For a large retail bank (~10% deposit market share), geographic
    # co-location with small/medium employers raises overlap above the
    # base rate. NFIB 2023: 56% of small business owners keep personal
    # and business accounts at the same bank. Large employers typically
    # use commercial banking or third-party payroll processors (ADP,
    # Paychex), so the blended in-bank rate is lower than for merchants.
    #
    # Estimate: ~4% of the employer pool banks at the same institution.
    employer_in_bank_p: float = field(default=0.04, metadata={"between": (0.0, 1.0)})

    # Probability that a freelance/small-business client payer also banks
    # at our institution. Clients are geographically diverse businesses,
    # so overlap is lower than local merchants or employers.
    #
    # Estimate: ~2% of the client pool banks at the same institution.
    client_in_bank_p: float = field(default=0.02, metadata={"between": (0.0, 1.0)})

    def __post_init__(self) -> None:
        validate_metadata(self)
