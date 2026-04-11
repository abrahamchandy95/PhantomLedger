"""
Insurance ownership and premium distributions by persona.

Sources:
- Car ownership: 92% of US households (Census 2024)
- Home insurance: avg $163/month (Ramsey 2025)
- Auto insurance: avg $225/month (Bankrate 2026)
- Life insurance: 52% of Americans (NW Mutual 2024)
- Auto claim rate: ~4.2 per 100 drivers/year (III 2024)
- Home claim rate: ~5-6% of policies/year (Triple-I 2024)
- Avg home claim payout: ~$15,749 (Triple-I 2024)
- Avg auto collision claim payout: ~$4,700 (III 2024)
"""

from dataclasses import dataclass, field

from common.validate import between, ge, gt


@dataclass(frozen=True, slots=True)
class PersonaRates:
    """Ownership probability per persona for each insurance type."""

    auto: float
    home: float
    life: float

    def __post_init__(self) -> None:
        between("auto", self.auto, 0.0, 1.0)
        between("home", self.home, 0.0, 1.0)
        between("life", self.life, 0.0, 1.0)


@dataclass(frozen=True, slots=True)
class Insurance:
    # --- premium distributions (monthly, USD) ---
    auto_median: float = 225.0
    auto_sigma: float = 0.35
    home_median: float = 163.0
    home_sigma: float = 0.40
    life_median: float = 40.0
    life_sigma: float = 0.50

    # --- claim probabilities (annual) ---
    auto_claim_annual_p: float = 0.042
    home_claim_annual_p: float = 0.055
    life_claim_p: float = 0.0  # death events not modeled

    # --- claim payout distributions ---
    auto_claim_median: float = 4700.0
    auto_claim_sigma: float = 0.80
    home_claim_median: float = 15750.0
    home_claim_sigma: float = 0.90

    # --- financed-collateral coverage anchors ---
    # These are intentionally high, not necessarily hard 1.0.
    # Set to 1.0 if you want "strict lender-required" behavior.
    mortgage_home_required_p: float = 0.998
    auto_loan_auto_required_p: float = 0.997

    # --- persona ownership rates ---
    rates: dict[str, PersonaRates] = field(
        default_factory=lambda: {
            "student": PersonaRates(auto=0.55, home=0.0, life=0.05),
            "retired": PersonaRates(auto=0.80, home=0.80, life=0.40),
            "salaried": PersonaRates(auto=0.90, home=0.55, life=0.55),
            "freelancer": PersonaRates(auto=0.85, home=0.35, life=0.30),
            "smallbiz": PersonaRates(auto=0.92, home=0.60, life=0.60),
            "hnw": PersonaRates(auto=0.95, home=0.90, life=0.80),
        }
    )

    def __post_init__(self) -> None:
        gt("auto_median", self.auto_median, 0.0)
        ge("auto_sigma", self.auto_sigma, 0.0)
        gt("home_median", self.home_median, 0.0)
        ge("home_sigma", self.home_sigma, 0.0)
        gt("life_median", self.life_median, 0.0)
        ge("life_sigma", self.life_sigma, 0.0)

        between("auto_claim_annual_p", self.auto_claim_annual_p, 0.0, 1.0)
        between("home_claim_annual_p", self.home_claim_annual_p, 0.0, 1.0)

        gt("auto_claim_median", self.auto_claim_median, 0.0)
        gt("home_claim_median", self.home_claim_median, 0.0)

        between("mortgage_home_required_p", self.mortgage_home_required_p, 0.0, 1.0)
        between(
            "auto_loan_auto_required_p",
            self.auto_loan_auto_required_p,
            0.0,
            1.0,
        )

    def for_persona(self, name: str) -> PersonaRates:
        return self.rates.get(name, self.rates["salaried"])
