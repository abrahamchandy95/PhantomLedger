from datetime import datetime

from common.random import Rng, SeedBank

from .growth import (
    as_seedbank,
    compound_growth,
    lognormal_multiplier,
    normal_clamped,
    pick_different_or_same,
    pick_one,
    sample_backdated_interval,
    sample_forward_interval,
    validated_seedbank,
)
from .models import EmploymentState
from .policy import RecurringPolicy
from .types import SalarySampler, SeedSource


def _require_employers(employers: list[str]) -> None:
    if not employers:
        raise ValueError("employers must be non-empty")


def salary_real_raise_for_year(
    policy: RecurringPolicy,
    seedbank: SeedBank,
    key: str,
    year: int,
) -> float:
    gen = seedbank.generator("salary_real_raise", key, str(year))
    return normal_clamped(
        gen,
        mu=policy.salary_real_raise_mu,
        sigma=policy.salary_real_raise_sigma,
        floor=policy.salary_real_raise_floor,
    )


def job_switch_bump(
    policy: RecurringPolicy,
    seedbank: SeedBank,
    person_id: str,
    switch_index: int,
) -> float:
    gen = seedbank.generator("job_switch_bump", person_id, str(switch_index))
    return normal_clamped(
        gen,
        mu=policy.job_switch_bump_mu,
        sigma=policy.job_switch_bump_sigma,
        floor=policy.job_switch_bump_floor,
    )


def compound_growth_salary(
    policy: RecurringPolicy,
    seedbank: SeedBank,
    person_id: str,
    start: datetime,
    now: datetime,
) -> float:
    return compound_growth(
        policy=policy,
        seedbank=seedbank,
        key=person_id,
        start=start,
        now=now,
        annual_real_raise=salary_real_raise_for_year,
    )


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
    _require_employers(employers)

    seedbank = validated_seedbank(policy, seed)
    gen = seedbank.generator("employment_init", person_id)

    employer = pick_one(gen, employers, name="employers")
    job_start, job_end = sample_backdated_interval(
        gen,
        anchor_date=start_date,
        years_min=policy.employer_tenure_years_min,
        years_max=policy.employer_tenure_years_max,
    )

    base_salary_raw = float(base_salary_sampler())
    base_salary = base_salary_raw * lognormal_multiplier(gen, sigma=0.03)

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
    _require_employers(employers)

    seedbank = as_seedbank(seed)

    employer = pick_different_or_same(
        rng,
        employers,
        prev.employer_acct,
        name="employers",
    )

    start, end = sample_forward_interval(
        rng.gen,
        start=now,
        years_min=policy.employer_tenure_years_min,
        years_max=policy.employer_tenure_years_max,
    )

    current_salary = prev.base_salary * compound_growth_salary(
        policy,
        seedbank,
        person_id,
        prev.start,
        now,
    )

    bump = job_switch_bump(policy, seedbank, person_id, prev.switch_index + 1)
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
    seedbank = as_seedbank(seed)
    amount = state.base_salary * compound_growth_salary(
        policy,
        seedbank,
        person_id,
        state.start,
        pay_date,
    )
    return round(max(50.0, float(amount)), 2)
