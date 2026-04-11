"""
Centralized amount sampling keyed on (channel, category).

Replaces the scattered amount functions in math_models/amounts.py
with a single registry that makes distribution drift impossible.
Each channel has exactly one declared model, and adding a new
channel without an amount model fails loudly at lookup time.

The previous implementation had a live bug: external_unknown outflows
used p2p_amount() (median $45, sigma 0.8), which is the wrong
distribution. External unknowns represent wire transfers, online
bill pay to unknown billers, and marketplace purchases — they should
be heavier-tailed and higher-median than casual P2P transfers.

Sources for distribution parameters:
- Grocery/fuel/restaurant: ClearlyPayments 2025, FMI 2024
- P2P: Fed Diary of Consumer Payment Choice 2024
- ATM: Federal Reserve Payments Study, ATM Marketplace
- External unknown: Fed Payments Study 2024 (non-card remote payments)
- Salary: BLS Quarterly Census of Employment and Wages 2024
- Fraud: calibrated to FATF Money Mule typology reports (2022)
- Rent/bills: Census AHS 2023, BLS CPI housing component
- Credit card: Fed G.19 release, TransUnion Q3 2024
"""

from dataclasses import dataclass

from common.math import lognormal_by_median
from common.random import Rng
from common.validate import ge, gt


@dataclass(frozen=True, slots=True)
class AmountModel:
    """A single lognormal amount distribution."""

    median: float
    sigma: float
    floor: float = 1.0

    def __post_init__(self) -> None:
        gt("median", self.median, 0.0)
        ge("sigma", self.sigma, 0.0)
        ge("floor", self.floor, 0.0)

    def sample(self, rng: Rng) -> float:
        amt = float(lognormal_by_median(rng.gen, median=self.median, sigma=self.sigma))
        return round(max(self.floor, amt), 2)


@dataclass(frozen=True, slots=True)
class GammaAmountModel:
    """A gamma-based amount distribution (used for bills)."""

    shape: float
    scale: float
    add: float
    floor: float = 1.0

    def __post_init__(self) -> None:
        gt("shape", self.shape, 0.0)
        gt("scale", self.scale, 0.0)
        ge("floor", self.floor, 0.0)

    def sample(self, rng: Rng) -> float:
        amt = float(rng.gen.gamma(shape=self.shape, scale=self.scale)) + self.add
        return round(max(self.floor, amt), 2)


# ---------------------------------------------------------------
# Channel-level models
# ---------------------------------------------------------------

SALARY = AmountModel(median=3000.0, sigma=0.35, floor=50.0)
RENT = GammaAmountModel(shape=2.0, scale=400.0, add=50.0, floor=1.0)
P2P = AmountModel(median=45.0, sigma=0.8, floor=1.0)
BILL = GammaAmountModel(shape=2.0, scale=400.0, add=50.0, floor=1.0)

# External unknowns include wire transfers, online bill pay to
# unknown billers, marketplace purchases, and miscellaneous.
# Fed Payments Study 2024: median non-card remote payment ~$120,
# with a heavy tail extending into thousands for wires.
EXTERNAL_UNKNOWN = AmountModel(median=120.0, sigma=0.95, floor=5.0)

ATM = AmountModel(median=80.0, sigma=0.30, floor=20.0)
SELF_TRANSFER = AmountModel(median=250.0, sigma=0.80, floor=10.0)
SUBSCRIPTION = AmountModel(median=15.0, sigma=0.40, floor=5.0)
CLIENT_ACH_CREDIT = AmountModel(median=1500.0, sigma=0.75, floor=50.0)
CARD_SETTLEMENT = AmountModel(median=650.0, sigma=0.60, floor=20.0)
PLATFORM_PAYOUT = AmountModel(median=400.0, sigma=0.65, floor=10.0)
OWNER_DRAW = AmountModel(median=2500.0, sigma=0.80, floor=100.0)
INVESTMENT_INFLOW = AmountModel(median=5000.0, sigma=1.00, floor=100.0)
# Fraud-specific
FRAUD = AmountModel(median=900.0, sigma=0.70, floor=50.0)
FRAUD_CYCLE = AmountModel(median=600.0, sigma=0.25, floor=1.0)
FRAUD_BOOST_CYCLE = AmountModel(median=500.0, sigma=0.20, floor=1.0)

# ---------------------------------------------------------------
# Merchant category models
# Sources: ClearlyPayments 2025, FMI 2024, Fed Diary 2024
# ---------------------------------------------------------------

MERCHANT_CATEGORY: dict[str, AmountModel] = {
    "grocery": AmountModel(median=50.0, sigma=0.55, floor=1.0),
    "fuel": AmountModel(median=45.0, sigma=0.35, floor=1.0),
    "restaurant": AmountModel(median=28.0, sigma=0.60, floor=1.0),
    "pharmacy": AmountModel(median=25.0, sigma=0.65, floor=1.0),
    "ecommerce": AmountModel(median=85.0, sigma=0.70, floor=1.0),
    "retail_other": AmountModel(median=45.0, sigma=0.75, floor=1.0),
    "utilities": AmountModel(median=120.0, sigma=0.40, floor=1.0),
    "telecom": AmountModel(median=75.0, sigma=0.30, floor=1.0),
    "insurance": AmountModel(median=150.0, sigma=0.35, floor=1.0),
    "education": AmountModel(median=200.0, sigma=0.60, floor=1.0),
}

_DEFAULT_MERCHANT = AmountModel(median=45.0, sigma=0.70, floor=1.0)


def merchant_amount(rng: Rng, category: str) -> float:
    """Sample a merchant transaction amount, category-aware."""
    model = MERCHANT_CATEGORY.get(category, _DEFAULT_MERCHANT)
    return model.sample(rng)


# ---------------------------------------------------------------
# Channel registry for programmatic lookup
# ---------------------------------------------------------------

_CHANNEL_MODELS: dict[str, AmountModel | GammaAmountModel] = {
    "salary": SALARY,
    "rent": RENT,
    "p2p": P2P,
    "bill": BILL,
    "external_unknown": EXTERNAL_UNKNOWN,
    "atm_withdrawal": ATM,
    "self_transfer": SELF_TRANSFER,
    "subscription": SUBSCRIPTION,
    "fraud_classic": FRAUD,
    "fraud_cycle": FRAUD_CYCLE,
    "client_ach_credit": CLIENT_ACH_CREDIT,
    "card_settlement": CARD_SETTLEMENT,
    "platform_payout": PLATFORM_PAYOUT,
    "owner_draw": OWNER_DRAW,
    "investment_inflow": INVESTMENT_INFLOW,
}


def for_channel(channel: str) -> AmountModel | GammaAmountModel:
    """
    Look up the amount model for a channel.

    Raises KeyError if the channel has no registered model —
    this is intentional. Adding a new channel without declaring
    its amount distribution should fail loudly, not silently
    inherit a wrong distribution.
    """
    model = _CHANNEL_MODELS.get(channel)
    if model is None:
        raise KeyError(
            f"No amount model registered for channel '{channel}'. "
            + f"Available: {sorted(_CHANNEL_MODELS.keys())}"
        )
    return model


def sample_for_channel(rng: Rng, channel: str) -> float:
    """Convenience: look up and sample in one call."""
    return for_channel(channel).sample(rng)
