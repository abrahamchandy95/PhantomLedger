"""
Insurance policy product adapter.

Wraps the existing common/config/population/insurance.py config
and transfers/insurance.py generator as products in the portfolio.

Insurance is structurally similar to credit cards in that:

1. It has a complex existing transfer generator (transfers/insurance.py)
   that handles premium schedules AND stochastic claim events.
2. Claims are reactive — they occur probabilistically based on annual
   rates, not on a fixed schedule that scheduled_events() could enumerate.
3. The config already exists and is validated in common/config.

Unlike loan products, this adapter does NOT implement scheduled_events().
The transfers/insurance.py generator remains the source of truth for
premium and claim transactions. This adapter exists so:
  - The portfolio is complete (every product a person holds is visible)
  - Downstream consumers (ML features, analytics) can check policy presence
  - Future life events can reference and modify coverage

Statistics:
- Auto: 92% of US households own a vehicle (Census 2024)
- Home: avg $163/month premium (Ramsey 2025)
- Auto: avg $225/month premium (Bankrate 2026)
- Life: 52% of Americans have coverage (NW Mutual 2024)
- Auto claims: 4.2 per 100 drivers/year (III 2024)
- Home claims: 5-6% of policies/year (Triple-I 2024)
"""

from dataclasses import dataclass

from common.externals import INS_AUTO, INS_HOME, INS_LIFE
from common.validate import between, gt


@dataclass(frozen=True, slots=True)
class InsurancePolicy:
    """A single active insurance policy for one coverage type."""

    policy_type: str  # "auto", "home", or "life"
    carrier_acct: str  # external carrier account
    monthly_premium: float
    billing_day: int  # 1-28
    annual_claim_p: float  # probability of a claim per year

    def __post_init__(self) -> None:
        if self.policy_type not in ("auto", "home", "life"):
            raise ValueError(
                "policy_type must be 'auto', 'home', or 'life', "
                + f"got {self.policy_type!r}"
            )
        gt("monthly_premium", self.monthly_premium, 0.0)
        between("billing_day", self.billing_day, 1, 28)
        between("annual_claim_p", self.annual_claim_p, 0.0, 1.0)


@dataclass(frozen=True, slots=True)
class InsuranceHoldings:
    """
    Complete insurance coverage for one person.

    Any subset of the three policy types may be None. A person
    with all three holds comprehensive coverage; a student might
    have only auto.
    """

    auto: InsurancePolicy | None = None
    home: InsurancePolicy | None = None
    life: InsurancePolicy | None = None

    def active_count(self) -> int:
        """How many distinct policies are held."""
        return sum(1 for p in (self.auto, self.home, self.life) if p is not None)

    def total_monthly_premium(self) -> float:
        """Sum of all active premiums."""
        total = 0.0
        if self.auto is not None:
            total += self.auto.monthly_premium
        if self.home is not None:
            total += self.home.monthly_premium
        if self.life is not None:
            total += self.life.monthly_premium
        return total

    def has_any(self) -> bool:
        return self.active_count() > 0


# --- Constructor helpers ---


def auto_policy(
    monthly_premium: float, billing_day: int, claim_p: float
) -> InsurancePolicy:
    return InsurancePolicy(
        policy_type="auto",
        carrier_acct=INS_AUTO,
        monthly_premium=monthly_premium,
        billing_day=billing_day,
        annual_claim_p=claim_p,
    )


def home_policy(
    monthly_premium: float, billing_day: int, claim_p: float
) -> InsurancePolicy:
    return InsurancePolicy(
        policy_type="home",
        carrier_acct=INS_HOME,
        monthly_premium=monthly_premium,
        billing_day=billing_day,
        annual_claim_p=claim_p,
    )


def life_policy(monthly_premium: float, billing_day: int) -> InsurancePolicy:
    return InsurancePolicy(
        policy_type="life",
        carrier_acct=INS_LIFE,
        monthly_premium=monthly_premium,
        billing_day=billing_day,
        annual_claim_p=0.0,  # death events not modeled
    )
