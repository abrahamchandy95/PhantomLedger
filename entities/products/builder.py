"""
Portfolio builder — assigns financial products to people.

For each person, consults their persona to determine ownership
probabilities, then samples product terms from the config
distributions. Credit card terms are extracted from the already-
issued CreditCards model rather than re-sampled, since card
issuance also creates accounts.

The builder is deterministic: given the same seed and population,
it produces identical portfolios. Each person gets an independent
RNG stream derived from their ID, so insertion order doesn't matter.
"""

from datetime import datetime, timedelta

import numpy as np

from common.config.population.insurance import Insurance as InsuranceConfig
from common.externals import (
    IRS_TREASURY,
    LENDER_AUTO,
    LENDER_MORTGAGE,
    SERVICER_STUDENT,
)
from common.math import lognormal_by_median
from common.random import RngFactory
from entities.models import CreditCards

from .auto_loan import AutoLoanConfig, AutoLoanTerms, DEFAULT_AUTO_LOAN_CONFIG
from .card_account import CardTerms, extract_all
from .insurance import (
    InsuranceHoldings,
    auto_policy,
    home_policy,
    life_policy,
)
from .mortgage import MortgageConfig, MortgageTerms, DEFAULT_MORTGAGE_CONFIG
from .portfolio import Portfolio, PortfolioRegistry
from .student_loan import (
    DEFAULT_STUDENT_LOAN_CONFIG,
    StudentLoanConfig,
    StudentLoanTerms,
)
from .tax_profile import DEFAULT_TAX_CONFIG, TaxConfig, TaxTerms


def _clamp01(value: float) -> float:
    return max(0.0, min(1.0, float(value)))


def _sample_payment_day(gen: np.random.Generator) -> int:
    """Sample a billing day in [1, 28]."""
    return int(gen.integers(1, 29))


def _residual_policy_probability(
    *,
    target_p: float,
    anchored_population_p: float,
    anchored_policy_p: float,
) -> float:
    """
    Back into the policy probability for the non-anchored population.

    Example:
      if 45% of a persona finances a car and financed cars receive
      insurance with probability 0.997, this returns the probability
      that the remaining non-financed group should receive auto coverage
      so that the overall insurance rate stays close to the configured
      persona target.
    """
    target = _clamp01(target_p)
    anchored_share = _clamp01(anchored_population_p)
    anchored_policy = _clamp01(anchored_policy_p)

    if target <= 0.0:
        return 0.0
    if anchored_share >= 1.0:
        return anchored_policy

    residual = (target - (anchored_share * anchored_policy)) / (1.0 - anchored_share)
    return _clamp01(residual)


def _try_mortgage(
    gen: np.random.Generator,
    cfg: MortgageConfig,
    persona_name: str,
    start_date: datetime,
) -> MortgageTerms | None:
    if float(gen.random()) >= cfg.ownership_p(persona_name):
        return None

    base = float(
        lognormal_by_median(gen, median=cfg.payment_median, sigma=cfg.payment_sigma)
    )
    payment = round(max(200.0, base * (1.0 + cfg.escrow_fraction)), 2)

    return MortgageTerms(
        monthly_payment=payment,
        payment_day=_sample_payment_day(gen),
        lender_acct=LENDER_MORTGAGE,
        start_date=start_date - timedelta(days=int(gen.integers(365, 3650))),
        term_months=360,
    )


def _try_auto_loan(
    gen: np.random.Generator,
    cfg: AutoLoanConfig,
    persona_name: str,
    start_date: datetime,
) -> AutoLoanTerms | None:
    if float(gen.random()) >= cfg.ownership_p(persona_name):
        return None

    payment = float(
        lognormal_by_median(gen, median=cfg.payment_median, sigma=cfg.payment_sigma)
    )
    payment = round(max(100.0, payment), 2)

    term = int(gen.integers(36, 85))

    return AutoLoanTerms(
        monthly_payment=payment,
        payment_day=_sample_payment_day(gen),
        lender_acct=LENDER_AUTO,
        start_date=start_date - timedelta(days=int(gen.integers(30, term * 30))),
        term_months=term,
    )


def _try_student_loan(
    gen: np.random.Generator,
    cfg: StudentLoanConfig,
    persona_name: str,
    start_date: datetime,
) -> StudentLoanTerms | None:
    if float(gen.random()) >= cfg.ownership_p(persona_name):
        return None

    payment = float(
        lognormal_by_median(gen, median=cfg.payment_median, sigma=cfg.payment_sigma)
    )
    payment = round(max(50.0, payment), 2)

    in_deferment = (
        persona_name == "student" and float(gen.random()) < cfg.student_deferment_p
    )

    return StudentLoanTerms(
        monthly_payment=payment,
        payment_day=_sample_payment_day(gen),
        servicer_acct=SERVICER_STUDENT,
        start_date=start_date - timedelta(days=int(gen.integers(180, 3650))),
        in_deferment=in_deferment,
    )


def _try_tax(
    gen: np.random.Generator,
    cfg: TaxConfig,
    persona_name: str,
) -> TaxTerms | None:
    # Layer 1: quarterly estimated-tax profile
    quarterly = 0.0
    if float(gen.random()) < cfg.estimated_payment_p(persona_name):
        quarterly = float(
            lognormal_by_median(
                gen,
                median=cfg.quarterly_median,
                sigma=cfg.quarterly_sigma,
            )
        )
        quarterly = round(max(100.0, quarterly), 2)

    # Layer 2: annual settlement profile
    refund_amount = 0.0
    refund_month = 3

    balance_due_amount = 0.0
    balance_due_month = 4

    settlement_roll = float(gen.random())

    if settlement_roll < float(cfg.refund_p):
        refund_amount = float(
            lognormal_by_median(
                gen,
                median=cfg.refund_median,
                sigma=cfg.refund_sigma,
            )
        )
        refund_amount = round(max(100.0, refund_amount), 2)
        refund_month = int(gen.integers(2, 6))  # Feb-May

    elif settlement_roll < float(cfg.refund_p) + float(cfg.balance_due_p):
        balance_due_amount = float(
            lognormal_by_median(
                gen,
                median=cfg.balance_due_median,
                sigma=cfg.balance_due_sigma,
            )
        )
        balance_due_amount = round(max(100.0, balance_due_amount), 2)
        balance_due_month = 4  # filing deadline month

    # No visible tax behavior at all for this filer in-window
    if quarterly <= 0.0 and refund_amount <= 0.0 and balance_due_amount <= 0.0:
        return None

    return TaxTerms(
        treasury_acct=IRS_TREASURY,
        quarterly_amount=quarterly,
        refund_amount=refund_amount,
        refund_month=refund_month,
        balance_due_amount=balance_due_amount,
        balance_due_month=balance_due_month,
    )


def _try_insurance(
    gen: np.random.Generator,
    cfg: InsuranceConfig,
    persona_name: str,
    *,
    mortgage: MortgageTerms | None,
    auto_loan: AutoLoanTerms | None,
    mortgage_anchor_p: float,
    auto_loan_anchor_p: float,
) -> InsuranceHoldings | None:
    """
    Sample insurance coverage for one person.

    Home and auto coverage are anchored to financed collateral:
      - mortgage holders receive home coverage with high configurable probability
      - auto-loan holders receive auto coverage with high configurable probability

    Non-financed owners can still receive coverage, but their residual issuance
    probability is backed out so the overall persona-level rate stays close to
    the configured target.
    """
    rates = cfg.for_persona(persona_name)

    auto = None
    home = None
    life = None

    auto_anchor_policy_p = float(cfg.auto_loan_auto_required_p)
    home_anchor_policy_p = float(cfg.mortgage_home_required_p)

    if auto_loan is not None:
        auto_issue_p = auto_anchor_policy_p
    else:
        auto_issue_p = _residual_policy_probability(
            target_p=float(rates.auto),
            anchored_population_p=auto_loan_anchor_p,
            anchored_policy_p=auto_anchor_policy_p,
        )

    if float(gen.random()) < auto_issue_p:
        premium = float(
            lognormal_by_median(gen, median=cfg.auto_median, sigma=cfg.auto_sigma)
        )
        auto = auto_policy(
            monthly_premium=round(max(30.0, premium), 2),
            billing_day=_sample_payment_day(gen),
            claim_p=float(cfg.auto_claim_annual_p),
        )

    if mortgage is not None:
        home_issue_p = home_anchor_policy_p
    else:
        home_issue_p = _residual_policy_probability(
            target_p=float(rates.home),
            anchored_population_p=mortgage_anchor_p,
            anchored_policy_p=home_anchor_policy_p,
        )

    if float(gen.random()) < home_issue_p:
        premium = float(
            lognormal_by_median(gen, median=cfg.home_median, sigma=cfg.home_sigma)
        )
        home = home_policy(
            monthly_premium=round(max(20.0, premium), 2),
            billing_day=_sample_payment_day(gen),
            claim_p=float(cfg.home_claim_annual_p),
        )

    if float(gen.random()) < float(rates.life):
        premium = float(
            lognormal_by_median(gen, median=cfg.life_median, sigma=cfg.life_sigma)
        )
        life = life_policy(
            monthly_premium=round(max(10.0, premium), 2),
            billing_day=_sample_payment_day(gen),
        )

    if auto is None and home is None and life is None:
        return None

    return InsuranceHoldings(auto=auto, home=home, life=life)


def build_portfolios(
    *,
    base_seed: int,
    persona_map: dict[str, str],
    credit_cards: CreditCards,
    insurance_cfg: InsuranceConfig,
    start_date: datetime,
    mortgage_cfg: MortgageConfig = DEFAULT_MORTGAGE_CONFIG,
    auto_loan_cfg: AutoLoanConfig = DEFAULT_AUTO_LOAN_CONFIG,
    student_loan_cfg: StudentLoanConfig = DEFAULT_STUDENT_LOAN_CONFIG,
    tax_cfg: TaxConfig = DEFAULT_TAX_CONFIG,
) -> PortfolioRegistry:
    """
    Assign financial products to every person in the population.

    Credit card terms are extracted from the already-issued CreditCards
    model (since issuance also creates accounts). Insurance, loans, and
    tax profiles are sampled fresh from their config distributions.

    Collateralized insurance is not sampled fully independently from the
    financed asset: mortgage and auto-loan holders are strongly anchored
    to matching home/auto coverage.
    """
    rng_factory = RngFactory(base_seed)

    card_terms_by_person: dict[str, CardTerms] = extract_all(credit_cards)
    portfolios: dict[str, Portfolio] = {}

    for person_id, persona_name in persona_map.items():
        gen = rng_factory.rng("portfolio", person_id).gen

        mortgage_anchor_p = float(mortgage_cfg.ownership_p(persona_name))
        auto_loan_anchor_p = float(auto_loan_cfg.ownership_p(persona_name))

        mortgage = _try_mortgage(gen, mortgage_cfg, persona_name, start_date)
        auto_loan = _try_auto_loan(gen, auto_loan_cfg, persona_name, start_date)
        student_loan = _try_student_loan(
            gen, student_loan_cfg, persona_name, start_date
        )
        tax = _try_tax(gen, tax_cfg, persona_name)
        insurance = _try_insurance(
            gen,
            insurance_cfg,
            persona_name,
            mortgage=mortgage,
            auto_loan=auto_loan,
            mortgage_anchor_p=mortgage_anchor_p,
            auto_loan_anchor_p=auto_loan_anchor_p,
        )
        card: CardTerms | None = card_terms_by_person.get(person_id)

        has_any = any(
            p is not None
            for p in (mortgage, auto_loan, student_loan, tax, card, insurance)
        )

        if has_any:
            portfolios[person_id] = Portfolio(
                person_id=person_id,
                mortgage=mortgage,
                auto_loan=auto_loan,
                student_loan=student_loan,
                tax=tax,
                credit_card=card,
                insurance=insurance,
            )

    return PortfolioRegistry(by_person=portfolios)
