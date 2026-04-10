from datetime import datetime

from common.random import RngFactory

from .growth import (
    build_rng_factory,
    compound_growth,
    pick_different,
    pick_one,
    sample_backdated_interval,
    sample_forward_interval,
    sample_lognormal_multiplier,
    sample_normal_clamped,
)
from .policy import Policy
from .state import Employment as State, SalarySource, SeedSource


def _require_employers(employers: list[str]) -> None:
    if not employers:
        raise ValueError("The employers list must be non-empty.")


def _salary_real_raise_for_year(
    policy: Policy,
    rng_factory: RngFactory,
    key: str,
    year: int,
) -> float:
    gen = rng_factory.rng("salary_real_raise", key, str(year)).gen
    return sample_normal_clamped(
        gen,
        mu=policy.salary_raise_mu,
        sigma=policy.salary_raise_sigma,
        floor=policy.salary_raise_floor,
    )


def _job_switch_bump(
    policy: Policy,
    rng_factory: RngFactory,
    person_id: str,
    switch_index: int,
) -> float:
    gen = rng_factory.rng("job_switch_bump", person_id, str(switch_index)).gen
    return sample_normal_clamped(
        gen,
        mu=policy.job_bump_mu,
        sigma=policy.job_bump_sigma,
        floor=policy.job_bump_floor,
    )


def _compound_growth_salary(
    policy: Policy,
    rng_factory: RngFactory,
    person_id: str,
    start: datetime,
    now: datetime,
) -> float:
    return compound_growth(
        policy=policy,
        rng_factory=rng_factory,
        key=person_id,
        start=start,
        now=now,
        annual_raise_source=_salary_real_raise_for_year,
    )


# --- Public API ---


def initialize(
    policy: Policy,
    seed: SeedSource,
    *,
    person_id: str,
    start_date: datetime,
    employers: list[str],
    base_salary_source: SalarySource,
) -> State:
    """
    Bootstraps the initial employment state for a person.
    """
    _require_employers(employers)

    rng_factory = build_rng_factory(seed)
    gen = rng_factory.rng("employment_init", person_id).gen

    employer = pick_one(gen, employers, name="employers")
    job_start, job_end = sample_backdated_interval(
        gen,
        anchor_date=start_date,
        years_min=policy.job_tenure_min,
        years_max=policy.job_tenure_max,
    )

    base_salary_raw = float(base_salary_source())
    base_salary = base_salary_raw * sample_lognormal_multiplier(gen, sigma=0.03)

    return State(
        employer_acct=employer,
        start=job_start,
        end=job_end,
        base_salary=base_salary,
        switch_index=0,
    )


def advance(
    policy: Policy,
    seed: SeedSource,
    *,
    person_id: str,
    now: datetime,
    employers: list[str],
    prev: State,
) -> State:
    """
    Transitions a person to a new job, applying tenure and salary bumps.
    """
    _require_employers(employers)

    rng_factory = build_rng_factory(seed)

    # advance uses its own derived stream for the employer pick
    adv_gen = rng_factory.rng(
        "employment_advance", person_id, str(prev.switch_index + 1)
    ).gen

    employer = pick_different(
        adv_gen,
        employers,
        prev.employer_acct,
        name="employers",
    )

    start, end = sample_forward_interval(
        adv_gen,
        start=now,
        years_min=policy.job_tenure_min,
        years_max=policy.job_tenure_max,
    )

    current_salary = prev.base_salary * _compound_growth_salary(
        policy,
        rng_factory,
        person_id,
        prev.start,
        now,
    )

    bump = _job_switch_bump(policy, rng_factory, person_id, prev.switch_index + 1)
    new_base = current_salary * (1.0 + bump)

    return State(
        employer_acct=employer,
        start=start,
        end=end,
        base_salary=new_base,
        switch_index=prev.switch_index + 1,
    )


def calculate_salary(
    policy: Policy,
    seed: SeedSource,
    *,
    person_id: str,
    state: State,
    pay_date: datetime,
) -> float:
    """
    Calculates the exact salary for a given pay period, including all compounded raises.
    """
    rng_factory = build_rng_factory(seed)
    amount = state.base_salary * _compound_growth_salary(
        policy,
        rng_factory,
        person_id,
        state.start,
        pay_date,
    )
    return round(max(50.0, float(amount)), 2)
