from collections.abc import Callable, Sequence
from dataclasses import dataclass
from datetime import datetime, timedelta

import numpy as np

from common.random import Rng, SeedBank
from common.validate import (
    require_float_between,
    require_float_ge,
    require_float_gt,
)

type SalarySampler = Callable[[], float]
type RentSampler = Callable[[], float]


@dataclass(frozen=True, slots=True)
class RecurringPolicy:
    salary_fraction: float = 0.65
    rent_fraction: float = 0.55

    employer_tenure_years_min: float = 2.0
    employer_tenure_years_max: float = 10.0
    landlord_tenure_years_min: float = 2.0
    landlord_tenure_years_max: float = 10.0

    annual_inflation_rate: float = 0.03

    salary_real_raise_mu: float = 0.02
    salary_real_raise_sigma: float = 0.01
    salary_real_raise_floor: float = 0.005

    job_switch_bump_mu: float = 0.08
    job_switch_bump_sigma: float = 0.05
    job_switch_bump_floor: float = 0.00

    rent_real_raise_mu: float = 0.03
    rent_real_raise_sigma: float = 0.02
    rent_real_raise_floor: float = 0.00

    def validate(self) -> None:
        require_float_between("salary_fraction", self.salary_fraction, 0.0, 1.0)
        require_float_between("rent_fraction", self.rent_fraction, 0.0, 1.0)

        require_float_gt(
            "employer_tenure_years_min",
            self.employer_tenure_years_min,
            0.0,
        )
        if float(self.employer_tenure_years_max) < float(
            self.employer_tenure_years_min
        ):
            raise ValueError(
                "employer_tenure_years_max must be >= employer_tenure_years_min"
            )

        require_float_gt(
            "landlord_tenure_years_min",
            self.landlord_tenure_years_min,
            0.0,
        )
        if float(self.landlord_tenure_years_max) < float(
            self.landlord_tenure_years_min
        ):
            raise ValueError(
                "landlord_tenure_years_max must be >= landlord_tenure_years_min"
            )

        require_float_ge("annual_inflation_rate", self.annual_inflation_rate, 0.0)
        require_float_ge("salary_real_raise_sigma", self.salary_real_raise_sigma, 0.0)
        require_float_ge("job_switch_bump_sigma", self.job_switch_bump_sigma, 0.0)
        require_float_ge("rent_real_raise_sigma", self.rent_real_raise_sigma, 0.0)


DEFAULT_RECURRING_POLICY = RecurringPolicy()

type SeedSource = SeedBank | int
type AnnualRaiseSampler = Callable[[RecurringPolicy, SeedBank, str, int], float]


def _as_seedbank(seed: SeedSource) -> SeedBank:
    if isinstance(seed, SeedBank):
        return seed
    return SeedBank(int(seed))


def _years_to_days(years: float) -> int:
    return int(round(years * 365.25))


def _draw_index(gen: np.random.Generator, hi: int) -> int:
    if hi <= 0:
        raise ValueError("upper bound must be > 0")
    return int(gen.integers(0, hi))


def _pick_one(gen: np.random.Generator, items: Sequence[str], *, name: str) -> str:
    if not items:
        raise ValueError(f"{name} must be non-empty")
    return items[_draw_index(gen, len(items))]


def _pick_different_or_same(
    rng: Rng,
    items: Sequence[str],
    current: str,
    *,
    name: str,
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
    tenure_days = _sample_tenure_days(gen, years_min, years_max)
    age_days = _draw_index(gen, max(1, tenure_days))

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


def _salary_real_raise_for_year(
    policy: RecurringPolicy,
    seedbank: SeedBank,
    key: str,
    year: int,
) -> float:
    gen = seedbank.generator("salary_real_raise", key, str(year))
    return _normal_clamped(
        gen,
        mu=policy.salary_real_raise_mu,
        sigma=policy.salary_real_raise_sigma,
        floor=policy.salary_real_raise_floor,
    )


def _rent_real_raise_for_year(
    policy: RecurringPolicy,
    seedbank: SeedBank,
    key: str,
    year: int,
) -> float:
    gen = seedbank.generator("rent_real_raise", key, str(year))
    return _normal_clamped(
        gen,
        mu=policy.rent_real_raise_mu,
        sigma=policy.rent_real_raise_sigma,
        floor=policy.rent_real_raise_floor,
    )


def _job_switch_bump(
    policy: RecurringPolicy,
    seedbank: SeedBank,
    person_id: str,
    switch_index: int,
) -> float:
    gen = seedbank.generator("job_switch_bump", person_id, str(switch_index))
    return _normal_clamped(
        gen,
        mu=policy.job_switch_bump_mu,
        sigma=policy.job_switch_bump_sigma,
        floor=policy.job_switch_bump_floor,
    )


def _compound_growth(
    *,
    policy: RecurringPolicy,
    seedbank: SeedBank,
    key: str,
    start: datetime,
    now: datetime,
    annual_real_raise: AnnualRaiseSampler,
) -> float:
    n = _anniversaries_passed(start, now)
    if n <= 0:
        return 1.0

    growth = 1.0
    for i in range(n):
        year = start.year + i
        real = annual_real_raise(policy, seedbank, key, year)
        growth *= 1.0 + policy.annual_inflation_rate + real

    return growth


def _compound_growth_salary(
    policy: RecurringPolicy,
    seedbank: SeedBank,
    person_id: str,
    start: datetime,
    now: datetime,
) -> float:
    return _compound_growth(
        policy=policy,
        seedbank=seedbank,
        key=person_id,
        start=start,
        now=now,
        annual_real_raise=_salary_real_raise_for_year,
    )


def _compound_growth_rent(
    policy: RecurringPolicy,
    seedbank: SeedBank,
    payer_acct: str,
    start: datetime,
    now: datetime,
) -> float:
    return _compound_growth(
        policy=policy,
        seedbank=seedbank,
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
    switch_index: int


@dataclass(slots=True)
class LeaseState:
    landlord_acct: str
    start: datetime
    end: datetime
    base_rent: float
    move_index: int


def init_employment(
    policy: RecurringPolicy,
    seed: SeedSource,
    rng: Rng,
    *,
    person_id: str,
    start_date: datetime,
    employers: list[str],
    base_salary_sampler: SalarySampler,
) -> EmploymentState:
    if not employers:
        raise ValueError("employers must be non-empty")

    policy.validate()
    seedbank = _as_seedbank(seed)
    gen = seedbank.generator("employment_init", person_id)

    employer = _pick_one(gen, employers, name="employers")
    job_start, job_end = _sample_backdated_interval(
        gen,
        anchor_date=start_date,
        years_min=policy.employer_tenure_years_min,
        years_max=policy.employer_tenure_years_max,
    )

    base_salary_raw = float(base_salary_sampler())
    base_salary = base_salary_raw * _lognormal_multiplier(gen, sigma=0.03)

    _ = rng.float()

    return EmploymentState(
        employer_acct=employer,
        start=job_start,
        end=job_end,
        base_salary=base_salary,
        switch_index=0,
    )


def advance_employment(
    policy: RecurringPolicy,
    seed: SeedSource,
    rng: Rng,
    *,
    person_id: str,
    now: datetime,
    employers: list[str],
    prev: EmploymentState,
) -> EmploymentState:
    if not employers:
        raise ValueError("employers must be non-empty")

    seedbank = _as_seedbank(seed)

    employer = _pick_different_or_same(
        rng,
        employers,
        prev.employer_acct,
        name="employers",
    )

    start, end = _sample_forward_interval(
        rng.gen,
        start=now,
        years_min=policy.employer_tenure_years_min,
        years_max=policy.employer_tenure_years_max,
    )

    current_salary = prev.base_salary * _compound_growth_salary(
        policy,
        seedbank,
        person_id,
        prev.start,
        now,
    )

    bump = _job_switch_bump(policy, seedbank, person_id, prev.switch_index + 1)
    new_base = current_salary * (1.0 + bump)

    return EmploymentState(
        employer_acct=employer,
        start=start,
        end=end,
        base_salary=new_base,
        switch_index=prev.switch_index + 1,
    )


def salary_at(
    policy: RecurringPolicy,
    seed: SeedSource,
    *,
    person_id: str,
    state: EmploymentState,
    pay_date: datetime,
) -> float:
    seedbank = _as_seedbank(seed)
    amount = state.base_salary * _compound_growth_salary(
        policy,
        seedbank,
        person_id,
        state.start,
        pay_date,
    )
    return round(max(50.0, float(amount)), 2)


def init_lease(
    policy: RecurringPolicy,
    seed: SeedSource,
    rng: Rng,
    *,
    payer_acct: str,
    start_date: datetime,
    landlords: list[str],
    base_rent_sampler: RentSampler,
) -> LeaseState:
    if not landlords:
        raise ValueError("landlords must be non-empty")

    policy.validate()
    seedbank = _as_seedbank(seed)
    gen = seedbank.generator("lease_init", payer_acct)

    landlord = _pick_one(gen, landlords, name="landlords")
    lease_start, lease_end = _sample_backdated_interval(
        gen,
        anchor_date=start_date,
        years_min=policy.landlord_tenure_years_min,
        years_max=policy.landlord_tenure_years_max,
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
    policy: RecurringPolicy,
    seed: SeedSource,
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

    seedbank = _as_seedbank(seed)

    landlord = _pick_different_or_same(
        rng,
        landlords,
        prev.landlord_acct,
        name="landlords",
    )

    start, end = _sample_forward_interval(
        rng.gen,
        start=now,
        years_min=policy.landlord_tenure_years_min,
        years_max=policy.landlord_tenure_years_max,
    )

    base_rent_raw = float(reset_rent_sampler())
    gen = seedbank.generator("lease_reset_rent", payer_acct, str(prev.move_index + 1))
    base_rent = base_rent_raw * _lognormal_multiplier(gen, sigma=0.05)

    return LeaseState(
        landlord_acct=landlord,
        start=start,
        end=end,
        base_rent=base_rent,
        move_index=prev.move_index + 1,
    )


def rent_at(
    policy: RecurringPolicy,
    seed: SeedSource,
    *,
    payer_acct: str,
    state: LeaseState,
    pay_date: datetime,
) -> float:
    seedbank = _as_seedbank(seed)
    amount = state.base_rent * _compound_growth_rent(
        policy,
        seedbank,
        payer_acct,
        state.start,
        pay_date,
    )
    return round(max(1.0, float(amount)), 2)
