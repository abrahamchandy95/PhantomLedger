"""
Insurance transfer generator.

Reads insurance policy terms from the portfolio (single source of
truth for ownership and premium amounts) and emits:
  1. Monthly premium payments on the policy's billing day
  2. Stochastic claim payouts sampled per year

The ownership decision and premium amount sampling happen in
entities/products/builder.py::_try_insurance(). This module only
handles the temporal scheduling of premium payments and the
stochastic generation of claim events.
"""

from collections.abc import Sequence
from datetime import date, datetime, time, timedelta
from typing import cast

import numpy as np

from common.channels import INSURANCE_CLAIM, INSURANCE_PREMIUM
from common.config import Window
from common.config.population.insurance import Insurance
from common.externals import INS_AUTO, INS_HOME
from common.math import lognormal_by_median
from common.random import Rng, derive_seed
from common.timeline import active_months
from common.transactions import Transaction
from entities.products.insurance import InsurancePolicy
from entities.products.portfolio import PortfolioRegistry
from transfers.factory import TransactionDraft, TransactionFactory


def _midnight(d: date) -> datetime:
    if isinstance(d, datetime):
        return d.replace(hour=0, minute=0, second=0, microsecond=0)
    return datetime.combine(d, time.min)


def generate(
    cfg: Insurance,
    window: Window,
    rng: Rng,
    txf: TransactionFactory,
    *,
    base_seed: int,
    portfolios: PortfolioRegistry,
    primary_accounts: dict[str, str],
) -> list[Transaction]:
    """
    Generate insurance premium and claim transactions from portfolio data.

    Reads policies from the portfolio (authoritative source for
    ownership and premium amounts) and produces the temporal stream
    of premium payments plus stochastic claim payouts.
    """
    start = window.start_date
    days = int(window.days)
    months = cast(list[date], active_months(start, days))
    start_dt = _midnight(start)
    end_dt = start_dt + timedelta(days=days)

    if not months:
        return []

    txns: list[Transaction] = []
    n_months = len(months)

    for person_id, portfolio in portfolios.by_person.items():
        if portfolio.insurance is None:
            continue

        acct = primary_accounts.get(person_id)
        if not acct:
            continue

        holdings = portfolio.insurance
        gen = np.random.default_rng(derive_seed(base_seed, "insurance", person_id))

        # Auto — premiums + possible claim
        if holdings.auto is not None:
            txns.extend(
                _monthly_premiums(
                    rng,
                    txf,
                    months,
                    start_dt,
                    end_dt,
                    acct,
                    holdings.auto,
                )
            )
            if _claim_occurred(gen, holdings.auto.annual_claim_p, n_months):
                payout = _sample_claim(
                    gen, cfg.auto_claim_median, cfg.auto_claim_sigma, floor=500.0
                )
                txns.extend(
                    _claim_payout(
                        rng,
                        txf,
                        start_dt,
                        days,
                        end_dt,
                        INS_AUTO,
                        acct,
                        payout,
                    )
                )

        # Home — claim behavior still applies to all insured homeowners.
        # For mortgaged households, treat the premium as escrowed inside the
        # mortgage payment rather than as a separate direct bank outflow.
        if holdings.home is not None:
            if portfolio.mortgage is None:
                txns.extend(
                    _monthly_premiums(
                        rng,
                        txf,
                        months,
                        start_dt,
                        end_dt,
                        acct,
                        holdings.home,
                    )
                )

            if _claim_occurred(gen, holdings.home.annual_claim_p, n_months):
                payout = _sample_claim(
                    gen, cfg.home_claim_median, cfg.home_claim_sigma, floor=1000.0
                )
                txns.extend(
                    _claim_payout(
                        rng,
                        txf,
                        start_dt,
                        days,
                        end_dt,
                        INS_HOME,
                        acct,
                        payout,
                    )
                )

        # Life — premiums only (death events not modeled)
        if holdings.life is not None:
            txns.extend(
                _monthly_premiums(
                    rng,
                    txf,
                    months,
                    start_dt,
                    end_dt,
                    acct,
                    holdings.life,
                )
            )

    return txns


def _claim_occurred(
    gen: np.random.Generator,
    annual_p: float,
    n_months: int,
) -> bool:
    if annual_p <= 0.0 or n_months <= 0:
        return False

    window_p = cast(float, 1.0 - (1.0 - annual_p) ** (n_months / 12.0))
    return bool(float(gen.random()) < window_p)


def _sample_claim(
    gen: np.random.Generator,
    median: float,
    sigma: float,
    *,
    floor: float,
) -> float:
    amt = float(lognormal_by_median(gen, median=median, sigma=sigma))
    return round(max(floor, amt), 2)


def _monthly_premiums(
    rng: Rng,
    txf: TransactionFactory,
    months: Sequence[date],
    start_dt: datetime,
    end_dt: datetime,
    person_acct: str,
    policy: InsurancePolicy,
) -> list[Transaction]:
    """Generate monthly premium payments for a single policy."""
    txns: list[Transaction] = []
    day = min(28, max(1, policy.billing_day))

    for month in months:
        base = _midnight(month)
        ts = base + timedelta(
            days=day - 1,
            hours=rng.int(0, 6),
            minutes=rng.int(0, 60),
        )
        if ts < start_dt or ts >= end_dt:
            continue

        txns.append(
            txf.make(
                TransactionDraft(
                    source=person_acct,
                    destination=policy.carrier_acct,
                    amount=policy.monthly_premium,
                    timestamp=ts,
                    channel=INSURANCE_PREMIUM,
                )
            )
        )

    return txns


def _claim_payout(
    rng: Rng,
    txf: TransactionFactory,
    start_dt: datetime,
    days: int,
    end_dt: datetime,
    src: str,
    dst: str,
    amt: float,
) -> list[Transaction]:
    """Generate a single claim payout from carrier to policyholder."""
    day_off = rng.int(0, max(1, days))
    ts = start_dt + timedelta(
        days=day_off,
        hours=rng.int(9, 17),
        minutes=rng.int(0, 60),
    )
    if ts >= end_dt:
        return []

    return [
        txf.make(
            TransactionDraft(
                source=src,
                destination=dst,
                amount=amt,
                timestamp=ts,
                channel=INSURANCE_CLAIM,
            )
        )
    ]
