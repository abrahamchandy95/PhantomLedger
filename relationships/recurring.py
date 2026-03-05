from collections.abc import Callable
from dataclasses import dataclass
from datetime import datetime, timedelta

import numpy as np

from common.config import RecurringConfig
from common.rng import Rng
from common.seedutil import derived_seed


type SalarySampler = Callable[[], float]
type RentSampler = Callable[[], float]


def _years_to_days(years: float) -> int:
    return int(round(years * 365.25))


def _local_gen(seed: int, *parts: str) -> np.random.Generator:
    """
    Local deterministic RNG derived from seed + parts.
    Useful to make per-entity decisions stable regardless of iteration order.
    """
    return np.random.default_rng(derived_seed(seed, *parts))


def _sample_tenure_days(
    rng: np.random.Generator, years_min: float, years_max: float
) -> int:
    # Uniform tenure in [min, max] years
    if years_max <= years_min:
        return max(1, _years_to_days(years_min))
    y = float(rng.uniform(years_min, years_max))
    return max(1, _years_to_days(y))


def _anniversaries_passed(start: datetime, now: datetime) -> int:
    years = now.year - start.year
    if (now.month, now.day) < (start.month, start.day):
        years -= 1
    return max(0, years)


def _normal_clamped(
    gen: np.random.Generator, *, mu: float, sigma: float, floor: float
) -> float:
    x = float(gen.normal(loc=mu, scale=sigma))
    return max(floor, x)


def _real_raise_for_year(
    rcfg: RecurringConfig, seed: int, key: str, year: int
) -> float:
    g = _local_gen(seed, "salary_real_raise", key, str(year))
    return _normal_clamped(
        g,
        mu=rcfg.salary_real_raise_mu,
        sigma=rcfg.salary_real_raise_sigma,
        floor=rcfg.salary_real_raise_floor,
    )


def _rent_real_raise_for_year(
    rcfg: RecurringConfig, seed: int, key: str, year: int
) -> float:
    g = _local_gen(seed, "rent_real_raise", key, str(year))
    return _normal_clamped(
        g,
        mu=rcfg.rent_real_raise_mu,
        sigma=rcfg.rent_real_raise_sigma,
        floor=rcfg.rent_real_raise_floor,
    )


def _job_switch_bump(
    rcfg: RecurringConfig, seed: int, person_id: str, switch_index: int
) -> float:
    g = _local_gen(seed, "job_switch_bump", person_id, str(switch_index))
    return _normal_clamped(
        g,
        mu=rcfg.job_switch_bump_mu,
        sigma=rcfg.job_switch_bump_sigma,
        floor=rcfg.job_switch_bump_floor,
    )


def _compound_growth_salary(
    rcfg: RecurringConfig, seed: int, person_id: str, start: datetime, now: datetime
) -> float:
    n = _anniversaries_passed(start, now)
    if n <= 0:
        return 1.0

    growth = 1.0
    for i in range(n):
        yr = start.year + i
        real = _real_raise_for_year(rcfg, seed, person_id, yr)
        growth *= 1.0 + rcfg.annual_inflation_rate + real
    return growth


def _compound_growth_rent(
    rcfg: RecurringConfig, seed: int, payer_acct: str, start: datetime, now: datetime
) -> float:
    n = _anniversaries_passed(start, now)
    if n <= 0:
        return 1.0

    growth = 1.0
    for i in range(n):
        yr = start.year + i
        real = _rent_real_raise_for_year(rcfg, seed, payer_acct, yr)
        growth *= 1.0 + rcfg.annual_inflation_rate + real
    return growth


@dataclass(slots=True)
class EmploymentState:
    employer_acct: str
    start: datetime
    end: datetime
    base_salary: float
    switch_index: int  # increments per job change


@dataclass(slots=True)
class LeaseState:
    landlord_acct: str
    start: datetime
    end: datetime
    base_rent: float
    move_index: int


def init_employment(
    rcfg: RecurringConfig,
    seed: int,
    rng: Rng,
    *,
    person_id: str,
    start_date: datetime,
    employers: list[str],
    base_salary_sampler: SalarySampler,
) -> EmploymentState:
    if not employers:
        raise ValueError("employers must be non-empty")

    # Deterministic-but-random per person: stable across iteration order
    g = _local_gen(seed, "employment_init", person_id)

    employer = employers[int(g.integers(0, len(employers)))]
    tenure_days = _sample_tenure_days(
        g, rcfg.employer_tenure_years_min, rcfg.employer_tenure_years_max
    )

    # Randomize age so switches can occur inside shorter simulations
    age_days = int(g.integers(0, max(1, tenure_days)))
    job_start = start_date - timedelta(days=age_days)
    job_end = job_start + timedelta(days=tenure_days)

    # Base salary comes from your model, plus tiny deterministic per-person multiplier
    base_salary_raw = float(base_salary_sampler())
    mult = float(g.lognormal(mean=0.0, sigma=0.03))  # ~3% dispersion; tweak as desired
    base_salary = base_salary_raw * mult

    # touch rng so callers relying on global rng advancement still behave predictably
    _ = rng.float()

    return EmploymentState(employer, job_start, job_end, base_salary, switch_index=0)


def advance_employment(
    rcfg: RecurringConfig,
    seed: int,
    rng: Rng,
    *,
    person_id: str,
    now: datetime,
    employers: list[str],
    prev: EmploymentState,
) -> EmploymentState:
    if not employers:
        raise ValueError("employers must be non-empty")

    # Choose a different employer if possible
    if len(employers) > 1:
        employer = prev.employer_acct
        while employer == prev.employer_acct:
            employer = rng.choice(employers)
    else:
        employer = prev.employer_acct

    tenure_days = _sample_tenure_days(
        rng.gen, rcfg.employer_tenure_years_min, rcfg.employer_tenure_years_max
    )

    current_salary = prev.base_salary * _compound_growth_salary(
        rcfg, seed, person_id, prev.start, now
    )

    bump = _job_switch_bump(rcfg, seed, person_id, prev.switch_index + 1)
    new_base = current_salary * (1.0 + bump)

    start = now
    end = start + timedelta(days=tenure_days)
    return EmploymentState(
        employer, start, end, new_base, switch_index=prev.switch_index + 1
    )


def salary_at(
    rcfg: RecurringConfig,
    seed: int,
    *,
    person_id: str,
    state: EmploymentState,
    pay_date: datetime,
) -> float:
    amt = state.base_salary * _compound_growth_salary(
        rcfg, seed, person_id, state.start, pay_date
    )
    return round(max(50.0, float(amt)), 2)


def init_lease(
    rcfg: RecurringConfig,
    seed: int,
    rng: Rng,
    *,
    payer_acct: str,
    start_date: datetime,
    landlords: list[str],
    base_rent_sampler: RentSampler,
) -> LeaseState:
    if not landlords:
        raise ValueError("landlords must be non-empty")

    g = _local_gen(seed, "lease_init", payer_acct)

    landlord = landlords[int(g.integers(0, len(landlords)))]
    tenure_days = _sample_tenure_days(
        g, rcfg.landlord_tenure_years_min, rcfg.landlord_tenure_years_max
    )

    age_days = int(g.integers(0, max(1, tenure_days)))
    lease_start = start_date - timedelta(days=age_days)
    lease_end = lease_start + timedelta(days=tenure_days)

    base_rent_raw = float(base_rent_sampler())
    mult = float(g.lognormal(mean=0.0, sigma=0.05))  # rent has a bit more dispersion
    base_rent = base_rent_raw * mult

    _ = rng.float()

    return LeaseState(landlord, lease_start, lease_end, base_rent, move_index=0)


def advance_lease(
    rcfg: RecurringConfig,
    seed: int,
    rng: Rng,
    *,
    payer_acct: str,
    now: datetime,
    landlords: list[str],
    prev: LeaseState,
    reset_rent_sampler: RentSampler,
) -> LeaseState:
    if not landlords:
        raise ValueError("landlords must be non-empty")

    if len(landlords) > 1:
        landlord = prev.landlord_acct
        while landlord == prev.landlord_acct:
            landlord = rng.choice(landlords)
    else:
        landlord = prev.landlord_acct

    tenure_days = _sample_tenure_days(
        rng.gen, rcfg.landlord_tenure_years_min, rcfg.landlord_tenure_years_max
    )

    base_rent_raw = float(reset_rent_sampler())
    g = _local_gen(seed, "lease_reset_rent", payer_acct, str(prev.move_index + 1))
    mult = float(g.lognormal(mean=0.0, sigma=0.05))
    base_rent = base_rent_raw * mult

    start = now
    end = start + timedelta(days=tenure_days)
    return LeaseState(landlord, start, end, base_rent, move_index=prev.move_index + 1)


def rent_at(
    rcfg: RecurringConfig,
    seed: int,
    *,
    payer_acct: str,
    state: LeaseState,
    pay_date: datetime,
) -> float:
    amt = state.base_rent * _compound_growth_rent(
        rcfg, seed, payer_acct, state.start, pay_date
    )
    return round(max(1.0, float(amt)), 2)
