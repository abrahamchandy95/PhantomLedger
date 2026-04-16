"""
Canonical landlord typology.

Unit-weighted shares are sourced from the 2021 Rental Housing Finance Survey
(reference year 2020), published jointly by HUD and the U.S. Census Bureau.
At the unit level the RHFS reports:
  - Individual investors: 37.6% of rental units
  - LLPs / LPs / LLCs:     40.4% of rental units
  - Everything else (real-estate corporations, REITs, trustees, tenant-in-
    common, nonprofits, housing cooperatives, etc.): ~22% of units

CRS R47332 also notes that many owners classified as LLP/LP/LLC are actually
individuals who wrapped a single-property holding in an LLC for liability or
tax reasons. We therefore split the RHFS LLC bucket into two synthetic halves:

  LLC_SMALL    ~ the mom-and-pop-in-LLC-wrapper slice. Behaves structurally
                 like an individual landlord (small counterparty, check/ACH/
                 Zelle mix) and only differs in that the account looks like a
                 business on the bank's books.

  CORPORATE    ~ mid/large LLC operators, institutional owners, REITs, real-
                 estate corporations, trustees, and other professionally
                 managed structures. Rent is collected almost exclusively via
                 ACH through a property-management portal.

The three categories are the granularity downstream rent routing needs to
produce channel heterogeneity. Adding finer splits (e.g. REIT vs. trustee)
would not change payment behavior and is intentionally avoided here.
"""

from typing import Final


INDIVIDUAL: Final[str] = "individual"
LLC_SMALL: Final[str] = "llc_small"
CORPORATE: Final[str] = "corporate"

ALL: Final[tuple[str, ...]] = (INDIVIDUAL, LLC_SMALL, CORPORATE)


# Unit-weighted distribution used when the simulation draws landlord types.
# INDIVIDUAL preserves the RHFS 37.6% individual-investor share (rounded).
# LLC_SMALL takes ~15% of the RHFS 40.4% LLC bucket (the mom-and-pop wrapper
# slice, informed by the Harvard JCHS "LLC gray zone" discussion).
# CORPORATE absorbs the remaining ~25% of the LLC bucket plus the ~22%
# institutional / REIT / real-estate-corporation / trustee / other slice.
DEFAULT_TYPE_SHARES: Final[dict[str, float]] = {
    INDIVIDUAL: 0.38,
    LLC_SMALL: 0.15,
    CORPORATE: 0.47,
}
