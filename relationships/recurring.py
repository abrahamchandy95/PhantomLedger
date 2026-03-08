from collections.abc import Callable, Sequence
from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

import numpy as np

from common.config import RecurringConfig
from common.rng import Rng
from common.seeding import derived_seed


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


def _pick_one(gen: np.random.Generator, items: Sequence[str], *, name: str) -> str:
    if not items:
        raise ValueError(f"{name} must be non-empty")
    idx = int(cast(int | np.integer, gen.integers(0, len(items))))
    return items[idx]


def _pick_different_or_same(
    rng: Rng, items: Sequence[str], current: str, *, name: str
) -> str:
    if not items:
        raise ValueError(f"{name} must be non-empty")
    if len(items) <= 1:
        return current

    chosen = current
    while chosen == current:
        chosen = rng.choice(items)
    return chosen


def _sample_tenure_days(
    gen: np.random.Generator,
    years_min: float,
    years_max: float,
) -> int:
    """
    Uniform tenure in [years_min, years_max] years.
    """
    if years_max <= years_min:
        return max(1, _years_to_days(years_min))

    years = float(gen.uniform(years_min, years_max))
    return max(1, _years_to_days(years))


def _sample_backdated_interval(
    gen: np.random.Generator,
    *,
    anchor_date: datetime,
    years_min: float,
    years_max: float,
) -> tuple[datetime, datetime]:
    """
    Create a [start, end) interval where the contract/job/lease is already
    somewhere inside its tenure at anchor_date.
    """
    tenure_days = _sample_tenure_days(gen, years_min, years_max)
    age_days = int(cast(int | np.integer, gen.integers(0, max(1, tenure_days))))

    start = anchor_date - timedelta(days=age_days)
    end = start + timedelta(days=tenure_days)
    return start, end


def _sample_forward_interval(
    gen: np.random.Generator,
    *,
    start: datetime,
    years_min: float,
    years_max: float,
) -> tuple[datetime, datetime]:
    tenure_days = _sample_tenure_days(gen, years_min, years_max)
    end = start + timedelta(days=tenure_days)
    return start, end


def _anniversaries_passed(start: datetime, now: datetime) -> int:
    years = now.year - start.year
    if (now.month, now.day) < (start.month, start.day):
        years -= 1
    return max(0, years)


def _normal_clamped(
    gen: np.random.Generator,
    *,
    mu: float,
    sigma: float,
    floor: float,
) -> float:
    x = float(gen.normal(loc=mu, scale=sigma))
    return max(floor, x)


def _lognormal_multiplier(gen: np.random.Generator, *, sigma: float) -> float:
    return float(gen.lognormal(mean=0.0, sigma=sigma))


def _real_raise_for_year(
    rcfg: RecurringConfig,
    seed: int,
    key: str,
    year: int,
) -> float:
    gen = _local_gen(seed, "salary_real_raise", key, str(year))
    return _normal_clamped(
        gen,
        mu=rcfg.salary_real_raise_mu,
        sigma=rcfg.salary_real_raise_sigma,
        floor=rcfg.salary_real_raise_floor,
    )


def _rent_real_raise_for_year(
    rcfg: RecurringConfig,
    seed: int,
    key: str,
    year: int,
) -> float:
    gen = _local_gen(seed, "rent_real_raise", key, str(year))
    return _normal_clamped(
        gen,
        mu=rcfg.rent_real_raise_mu,
        sigma=rcfg.rent_real_raise_sigma,
        floor=rcfg.rent_real_raise_floor,
    )


def _job_switch_bump(
    rcfg: RecurringConfig,
    seed: int,
    person_id: str,
    switch_index: int,
) -> float:
    gen = _local_gen(seed, "job_switch_bump", person_id, str(switch_index))
    return _normal_clamped(
        gen,
        mu=rcfg.job_switch_bump_mu,
        sigma=rcfg.job_switch_bump_sigma,
        floor=rcfg.job_switch_bump_floor,
    )


def _compound_growth(
    *,
    rcfg: RecurringConfig,
    seed: int,
    key: str,
    start: datetime,
    now: datetime,
    annual_real_raise: Callable[[RecurringConfig, int, str, int], float],
) -> float:
    n = _anniversaries_passed(start, now)
    if n <= 0:
        return 1.0

    growth = 1.0
    for i in range(n):
        year = start.year + i
        real = annual_real_raise(rcfg, seed, key, year)
        growth *= 1.0 + rcfg.annual_inflation_rate + real

    return growth


def _compound_growth_salary(
    rcfg: RecurringConfig,
    seed: int,
    person_id: str,
    start: datetime,
    now: datetime,
) -> float:
    return _compound_growth(
        rcfg=rcfg,
        seed=seed,
        key=person_id,
        start=start,
        now=now,
        annual_real_raise=_real_raise_for_year,
    )


def _compound_growth_rent(
    rcfg: RecurringConfig,
    seed: int,
    payer_acct: str,
    start: datetime,
    now: datetime,
) -> float:
    return _compound_growth(
        rcfg=rcfg,
        seed=seed,
        key=payer_acct,
        start=start,
        now=now,
        annual_real_raise=_rent_real_raise_for_year,
    )


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

    gen = _local_gen(seed, "employment_init", person_id)

    employer = _pick_one(gen, employers, name="employers")
    job_start, job_end = _sample_backdated_interval(
        gen,
        anchor_date=start_date,
        years_min=rcfg.employer_tenure_years_min,
        years_max=rcfg.employer_tenure_years_max,
    )

    base_salary_raw = float(base_salary_sampler())
    base_salary = base_salary_raw * _lognormal_multiplier(gen, sigma=0.03)

    # touch rng so callers relying on global rng advancement still behave predictably
    _ = rng.float()

    return EmploymentState(
        employer_acct=employer,
        start=job_start,
        end=job_end,
        base_salary=base_salary,
        switch_index=0,
    )


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

    employer = _pick_different_or_same(
        rng,
        employers,
        prev.employer_acct,
        name="employers",
    )

    start, end = _sample_forward_interval(
        rng.gen,
        start=now,
        years_min=rcfg.employer_tenure_years_min,
        years_max=rcfg.employer_tenure_years_max,
    )

    current_salary = prev.base_salary * _compound_growth_salary(
        rcfg,
        seed,
        person_id,
        prev.start,
        now,
    )

    bump = _job_switch_bump(rcfg, seed, person_id, prev.switch_index + 1)
    new_base = current_salary * (1.0 + bump)

    return EmploymentState(
        employer_acct=employer,
        start=start,
        end=end,
        base_salary=new_base,
        switch_index=prev.switch_index + 1,
    )


def salary_at(
    rcfg: RecurringConfig,
    seed: int,
    *,
    person_id: str,
    state: EmploymentState,
    pay_date: datetime,
) -> float:
    amount = state.base_salary * _compound_growth_salary(
        rcfg,
        seed,
        person_id,
        state.start,
        pay_date,
    )
    return round(max(50.0, float(amount)), 2)


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

    gen = _local_gen(seed, "lease_init", payer_acct)

    landlord = _pick_one(gen, landlords, name="landlords")
    lease_start, lease_end = _sample_backdated_interval(
        gen,
        anchor_date=start_date,
        years_min=rcfg.landlord_tenure_years_min,
        years_max=rcfg.landlord_tenure_years_max,
    )

    base_rent_raw = float(base_rent_sampler())
    base_rent = base_rent_raw * _lognormal_multiplier(gen, sigma=0.05)

    _ = rng.float()

    return LeaseState(
        landlord_acct=landlord,
        start=lease_start,
        end=lease_end,
        base_rent=base_rent,
        move_index=0,
    )


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

    landlord = _pick_different_or_same(
        rng,
        landlords,
        prev.landlord_acct,
        name="landlords",
    )

    start, end = _sample_forward_interval(
        rng.gen,
        start=now,
        years_min=rcfg.landlord_tenure_years_min,
        years_max=rcfg.landlord_tenure_years_max,
    )

    base_rent_raw = float(reset_rent_sampler())
    gen = _local_gen(seed, "lease_reset_rent", payer_acct, str(prev.move_index + 1))
    base_rent = base_rent_raw * _lognormal_multiplier(gen, sigma=0.05)

    return LeaseState(
        landlord_acct=landlord,
        start=start,
        end=end,
        base_rent=base_rent,
        move_index=prev.move_index + 1,
    )


def rent_at(
    rcfg: RecurringConfig,
    seed: int,
    *,
    payer_acct: str,
    state: LeaseState,
    pay_date: datetime,
) -> float:
    amount = state.base_rent * _compound_growth_rent(
        rcfg,
        seed,
        payer_acct,
        state.start,
        pay_date,
    )
    return round(max(1.0, float(amount)), 2)
