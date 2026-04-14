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

Research / modeling notes:
- Mortgages are modeled as contractual 30-year loans, but sampled loan age
  is capped to roughly the effective duration range documented in mortgage
  research, since many mortgages refinance or terminate long before year 30.
- Auto loans preserve a new-vs-used segment split so payment sizes and term
  lengths reflect the research in auto_loan.py rather than one blended pool.
- Student loans preserve grace periods, repayment plans, maturity, and
  graduation-season seasonality so the implementation matches the research
  in student_loan.py.
"""

from datetime import datetime, timedelta

import numpy as np

from common.config.population.insurance import Insurance as InsuranceConfig
from common.date_math import add_months
from common.externals import (
    IRS_TREASURY,
    LENDER_AUTO,
    LENDER_MORTGAGE,
    SERVICER_STUDENT,
)
from common.math import lognormal_by_median
from common.persona_names import STUDENT
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


def _sample_mortgage_payment_day(gen: np.random.Generator) -> int:
    """
    Mortgages are heavily concentrated near the 1st of the month.
    """
    if float(gen.random()) < 0.85:
        return 1
    return int(gen.integers(2, 6))


def _sample_mortgage_age_days(gen: np.random.Generator) -> int:
    """
    Sample observed mortgage age at simulation start.

    Contractual mortgages are commonly 30 years, but the average life of a
    mortgage is materially shorter due to refinances and moves. We therefore
    limit sampled loan age to roughly the first decade and bias toward the
    middle of that interval.
    """
    raw_years = float(gen.triangular(left=0.5, mode=5.0, right=10.0))
    return max(30, int(round(raw_years * 365.0)))


def _sample_auto_segment(
    gen: np.random.Generator,
    cfg: AutoLoanConfig,
) -> str:
    """
    Sample whether this financed vehicle is modeled as new or used.

    Financing rates for new vs used vehicles are purchase/origination-style
    statistics. ownership_p() remains a stock-style prior over who currently
    carries an auto loan, and this helper chooses segment conditional on
    already having a loan.
    """
    if float(gen.random()) < float(cfg.new_vehicle_share):
        return "new"
    return "used"


def _sample_auto_term_months(
    gen: np.random.Generator,
    cfg: AutoLoanConfig,
    vehicle_segment: str,
) -> int:
    """
    Sample a segment-specific contractual auto-loan term using a truncated
    normal around the research-backed average term.
    """
    mean_months, sigma_months = cfg.term_params(vehicle_segment)
    raw = float(gen.normal(loc=mean_months, scale=sigma_months))
    term = int(round(raw))
    return max(int(cfg.term_min_months), min(int(cfg.term_max_months), term))


def _sample_student_plan(
    gen: np.random.Generator,
    cfg: StudentLoanConfig,
) -> tuple[str, int]:
    """
    Sample plan_type and contractual repayment term.

    Uses the plan mix defined in student_loan.py so the builder stays aligned
    with that module's research notes.
    """
    total = float(cfg.standard_plan_p + cfg.extended_plan_p + cfg.idr_like_plan_p)
    u = float(gen.random()) * total

    if u < float(cfg.standard_plan_p):
        return "standard", int(cfg.standard_term_months)

    u -= float(cfg.standard_plan_p)
    if u < float(cfg.extended_plan_p):
        return "extended", int(cfg.extended_term_months)

    idr_term = (
        int(cfg.idr_20_year_term_months)
        if float(gen.random()) < float(cfg.idr_20_year_p)
        else int(cfg.idr_25_year_term_months)
    )
    return "idr_like", idr_term


def _sample_student_exit_date(
    gen: np.random.Generator,
    *,
    start_date: datetime,
    grace_period_months: int,
    repayment_in_future: bool,
) -> datetime:
    """
    Sample a school-exit / graduation-like anchor date.

    We intentionally bias toward May/June with a secondary December cycle
    to preserve the research note about graduation-season seasonality
    without building a full academic-calendar model.
    """
    month_roll = float(gen.random())
    if month_roll < 0.45:
        month = 5
    elif month_roll < 0.80:
        month = 6
    else:
        month = 12

    year_offset = int(gen.integers(0, 5 if repayment_in_future else 8))
    year = (
        start_date.year + year_offset
        if repayment_in_future
        else start_date.year - year_offset
    )
    day = int(gen.integers(10, 29))

    exit_date = datetime(year, month, day)
    repayment_start = add_months(exit_date, grace_period_months)

    if repayment_in_future:
        while repayment_start <= start_date:
            exit_date = exit_date.replace(year=exit_date.year + 1)
            repayment_start = add_months(exit_date, grace_period_months)
    else:
        while repayment_start > start_date:
            exit_date = exit_date.replace(year=exit_date.year - 1)
            repayment_start = add_months(exit_date, grace_period_months)

    return exit_date


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

    # payment_median is treated as the total contractual monthly payment.
    # Because the mortgage research anchor already refers to the observed
    # monthly payment, we do not multiply by escrow_fraction again here.
    payment = float(
        lognormal_by_median(gen, median=cfg.payment_median, sigma=cfg.payment_sigma)
    )
    payment = round(max(200.0, payment), 2)

    mortgage_age_days = _sample_mortgage_age_days(gen)

    return MortgageTerms(
        monthly_payment=payment,
        payment_day=_sample_mortgage_payment_day(gen),
        lender_acct=LENDER_MORTGAGE,
        start_date=start_date - timedelta(days=mortgage_age_days),
        term_months=360,
        late_p=cfg.late_p,
        late_days_min=cfg.late_days_min,
        late_days_max=cfg.late_days_max,
        miss_p=cfg.miss_p,
        partial_p=cfg.partial_p,
        cure_p=cfg.cure_p,
        cluster_mult=cfg.cluster_mult,
        partial_min_frac=cfg.partial_min_frac,
        partial_max_frac=cfg.partial_max_frac,
        max_cure_cycles=cfg.max_cure_cycles,
    )


def _try_auto_loan(
    gen: np.random.Generator,
    cfg: AutoLoanConfig,
    persona_name: str,
    start_date: datetime,
) -> AutoLoanTerms | None:
    if float(gen.random()) >= cfg.ownership_p(persona_name):
        return None

    vehicle_segment = _sample_auto_segment(gen, cfg)

    payment_median, payment_sigma = cfg.payment_params(vehicle_segment)
    payment = float(
        lognormal_by_median(
            gen,
            median=payment_median,
            sigma=payment_sigma,
        )
    )
    payment = round(max(100.0, payment), 2)

    term_months = _sample_auto_term_months(gen, cfg, vehicle_segment)

    # Sample loan seasoning so some loans are relatively new while others
    # are already deep into repayment when the simulation window opens.
    max_age_days = max(30, term_months * 30)
    age_days = int(gen.integers(30, max_age_days + 1))

    return AutoLoanTerms(
        monthly_payment=payment,
        payment_day=_sample_payment_day(gen),
        lender_acct=LENDER_AUTO,
        start_date=start_date - timedelta(days=age_days),
        term_months=term_months,
        vehicle_segment=vehicle_segment,
        late_p=cfg.late_p,
        late_days_min=cfg.late_days_min,
        late_days_max=cfg.late_days_max,
        miss_p=cfg.miss_p,
        partial_p=cfg.partial_p,
        cure_p=cfg.cure_p,
        cluster_mult=cfg.cluster_mult,
        partial_min_frac=cfg.partial_min_frac,
        partial_max_frac=cfg.partial_max_frac,
        max_cure_cycles=cfg.max_cure_cycles,
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

    plan_type, term_months = _sample_student_plan(gen, cfg)

    if persona_name == STUDENT:
        still_deferred = float(gen.random()) < float(cfg.student_deferment_p)

        school_exit_date = _sample_student_exit_date(
            gen,
            start_date=start_date,
            grace_period_months=int(cfg.grace_period_months),
            repayment_in_future=still_deferred,
        )
        repayment_start_date = add_months(
            school_exit_date,
            int(cfg.grace_period_months),
        )
        deferment_end_date = school_exit_date if still_deferred else None

        # Origination can be months to several years before leaving school.
        origination_date = school_exit_date - timedelta(
            days=int(gen.integers(180, (4 * 365) + 1))
        )
    else:
        # Non-student personas usually already left school and are somewhere
        # inside the repayment horizon when the simulation begins.
        repayment_age_months = int(gen.integers(1, term_months + 1))
        repayment_start_date = start_date - timedelta(days=repayment_age_months * 30)

        # Approximate school exit to preserve the notion that repayment begins
        # only after a grace window following school exit.
        school_exit_date = repayment_start_date - timedelta(
            days=int(cfg.grace_period_months) * 30
        )
        deferment_end_date = None
        origination_date = school_exit_date - timedelta(
            days=int(gen.integers(180, (4 * 365) + 1))
        )

    maturity_date = add_months(repayment_start_date, term_months)

    return StudentLoanTerms(
        monthly_payment=payment,
        payment_day=_sample_payment_day(gen),
        servicer_acct=SERVICER_STUDENT,
        origination_date=origination_date,
        repayment_start_date=repayment_start_date,
        term_months=term_months,
        plan_type=plan_type,
        maturity_date=maturity_date,
        deferment_end_date=deferment_end_date,
        late_p=cfg.late_p,
        late_days_min=cfg.late_days_min,
        late_days_max=cfg.late_days_max,
        miss_p=cfg.miss_p,
        partial_p=cfg.partial_p,
        cure_p=cfg.cure_p,
        cluster_mult=cfg.cluster_mult,
        partial_min_frac=cfg.partial_min_frac,
        partial_max_frac=cfg.partial_max_frac,
        max_cure_cycles=cfg.max_cure_cycles,
    )


def _try_tax(
    gen: np.random.Generator,
    cfg: TaxConfig,
    persona_name: str,
) -> TaxTerms | None:
    quarterly = 0.0
    if float(gen.random()) < cfg.ownership_p(persona_name):
        quarterly = float(
            lognormal_by_median(
                gen,
                median=cfg.quarterly_amount_median,
                sigma=cfg.quarterly_amount_sigma,
            )
        )
        quarterly = round(max(100.0, quarterly), 2)

    refund_amount = 0.0
    refund_month = 3

    balance_due_amount = 0.0
    balance_due_month = 4

    settlement_roll = float(gen.random())

    if settlement_roll < float(cfg.refund_p):
        refund_amount = float(
            lognormal_by_median(
                gen,
                median=cfg.refund_amount_median,
                sigma=cfg.refund_amount_sigma,
            )
        )
        refund_amount = round(max(100.0, refund_amount), 2)
        refund_month = int(gen.integers(2, 6))

    elif settlement_roll < float(cfg.refund_p) + float(cfg.balance_due_p):
        balance_due_amount = float(
            lognormal_by_median(
                gen,
                median=cfg.balance_due_amount_median,
                sigma=cfg.balance_due_amount_sigma,
            )
        )
        balance_due_amount = round(max(100.0, balance_due_amount), 2)
        balance_due_month = 4

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
            gen,
            student_loan_cfg,
            persona_name,
            start_date,
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
