"""
Seasonal spending multipliers for retail and discretionary transactions.

Applies a monthly multiplier to the day-to-day transaction rate to
capture calendar-driven spending patterns. These are universal across
the population — everyone spends more in December — though persona-
specific amplification could be added later.

The multipliers are normalized so the annual mean equals 1.0: a year
simulated with seasonal patterns enabled produces the same total
transaction count as one without, just redistributed across months.
This means the feature adds realism without inflating baseline volume.

Sources:
  - National Retail Federation (NRF): "November and December account
    for about 20% of annual retail spending" (ASU 2025 analysis).
    Holiday retail sales $976B in 2024, $1.01T projected 2025.
  - Bank of America Consumer Checkpoint (2024-2025): retail spending
    peaks in November-December, trough in January-February.
  - JPMorgan Chase Institute (2024): "families experience large income
    swings in February, March, April, and December" — tax refunds
    and holiday spending respectively.
  - S&P Market Intelligence (2026): "10% increase in refunds is
    associated with ~2% rise in retail spending at general merchandise,
    apparel, furniture, and other stores."
  - IRS 2024-25: ~$329B in refunds issued, two-thirds before April 15.
    ~72% of filers receive refunds averaging $3,500+.
  - NRF Back-to-School (2025): $128B total spending, $858 avg per
    K-12 household. Third-largest seasonal event after holidays and
    back-to-college.
  - Visa Holiday Retail Report (2025): holiday retail +4.2% YoY;
    electronics +5.8%, apparel +5.3%.
  - Consumer spending consistently ~70% of US GDP (BEA).

Note on seasonality and mule detection:
  Real consumers have seasonal ramps. Mule accounts show a different
  pattern: a tight burst window with no calendar alignment. When the
  legitimate population has seasonal variation, the mule burst pattern
  becomes more distinguishable against the backdrop, not less. This
  is the core value of seasonality for the use case.
"""

from dataclasses import dataclass

from common.validate import ge


# Month-index (1-12) mean multipliers relative to 1.0 annual baseline.
# Calibrated from the sources above. The array has index 0 unused so
# month numbers map directly: _MONTHLY[month] where month in 1..12.
#
# Derivation:
#   Dec: 1.22  — peak holiday spending (~12% of annual vs 8.3% baseline)
#   Nov: 1.16  — Black Friday + early holiday
#   Jan: 0.88  — post-holiday trough, "dry January"
#   Feb: 0.94  — still low, but tax refund wave starts late month
#   Mar: 1.04  — refund spending peak (two-thirds of refunds by mid-April)
#   Apr: 1.02  — late refund spending, final tax season
#   May: 1.00  — neutral (Mother's Day offsets Memorial Day week drag)
#   Jun: 0.98  — summer, Father's Day modest bump
#   Jul: 0.97  — mid-summer, Independence Day
#   Aug: 1.05  — back-to-school ramp-up
#   Sep: 1.02  — tail of back-to-school
#   Oct: 0.99  — Halloween + holiday pre-season build
#
# These values are deliberately moderate. Real retail variance per
# category can be 3-5x the baseline during peak weeks, but the
# day-to-day generator is modeling *all* transactions, not just retail.
# Bills, rent, salaries, P2P don't seasonally fluctuate.

_MONTHLY_RAW: tuple[float, ...] = (
    0.0,  # index 0 unused
    0.88,  # Jan
    0.94,  # Feb
    1.04,  # Mar
    1.02,  # Apr
    1.00,  # May
    0.98,  # Jun
    0.97,  # Jul
    1.05,  # Aug
    1.02,  # Sep
    0.99,  # Oct
    1.16,  # Nov
    1.22,  # Dec
)


def _normalize_to_unit_mean(raw: tuple[float, ...]) -> tuple[float, ...]:
    """
    Rescale monthly multipliers so the mean across Jan-Dec equals 1.0.

    Guarantees that enabling seasonality doesn't change the expected
    annual transaction count — only redistributes it across months.
    """
    monthly = list(raw[1:])  # drop sentinel index 0
    current_mean = sum(monthly) / 12.0
    if current_mean <= 0.0:
        return raw
    scale = 1.0 / current_mean
    normalized = [0.0] + [m * scale for m in monthly]
    return tuple(normalized)


_MONTHLY: tuple[float, ...] = _normalize_to_unit_mean(_MONTHLY_RAW)


@dataclass(frozen=True, slots=True)
class SeasonalConfig:
    """
    Configuration for seasonal spending multipliers.

    intensity: Scale factor on the deviation from 1.0.
        0.0 = fully disabled (multiplier always 1.0)
        1.0 = default calibrated values
        > 1.0 = amplified seasonality (useful for testing sensitivity)

    enabled: Global toggle. When False, always returns 1.0.
    """

    enabled: bool = True
    intensity: float = 1.0

    def __post_init__(self) -> None:
        ge("intensity", self.intensity, 0.0)


DEFAULT_SEASONAL_CONFIG = SeasonalConfig()


def monthly_multiplier(
    month: int, cfg: SeasonalConfig = DEFAULT_SEASONAL_CONFIG
) -> float:
    """
    Get the seasonal spending multiplier for a given calendar month.

    Parameters
    ----------
    month : int
        Calendar month, 1-12.
    cfg : SeasonalConfig
        Controls whether seasonality is applied and at what intensity.

    Returns
    -------
    float
        Multiplier to apply to the daily transaction rate.
        1.0 means no adjustment; > 1.0 means boost; < 1.0 means damped.
    """
    if not cfg.enabled:
        return 1.0
    if not (1 <= month <= 12):
        return 1.0

    base = _MONTHLY[month]

    # intensity scales the deviation from 1.0:
    #   intensity=0 -> always 1.0
    #   intensity=1 -> use _MONTHLY as-is
    #   intensity=2 -> double the deviation
    return 1.0 + (base - 1.0) * float(cfg.intensity)


def daily_multiplier(
    day_of_year: int,
    year: int,
    cfg: SeasonalConfig = DEFAULT_SEASONAL_CONFIG,
) -> float:
    """
    Convenience: convert day-of-year to month and return the multiplier.

    Computes which calendar month a given day-of-year falls into,
    accounting for leap years, and returns the seasonal multiplier
    for that month.
    """
    from datetime import date, timedelta

    if not cfg.enabled:
        return 1.0

    # day_of_year is 0-indexed from Jan 1 of `year`
    dt = date(year, 1, 1) + timedelta(days=int(day_of_year))
    return monthly_multiplier(dt.month, cfg)
