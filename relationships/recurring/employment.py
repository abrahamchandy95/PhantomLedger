import calendar
from datetime import datetime, timedelta

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
from .state import (
    Employment as State,
    PayCadence,
    PayrollProfile,
    SalarySource,
    SeedSource,
)


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


def _midnight(ts: datetime) -> datetime:
    return datetime(ts.year, ts.month, ts.day)


def _month_day(year: int, month: int, day: int) -> datetime:
    last_day = calendar.monthrange(year, month)[1]
    return datetime(year, month, min(day, last_day))


def _roll_weekend(ts: datetime, mode: str) -> datetime:
    if mode == "none":
        return ts

    out = _midnight(ts)
    step = -1 if mode == "previous_business_day" else 1
    while out.weekday() >= 5:
        out += timedelta(days=step)
    return out


def _next_weekday_on_or_after(ts: datetime, weekday: int) -> datetime:
    ts = _midnight(ts)
    delta = (int(weekday) - ts.weekday()) % 7
    return ts + timedelta(days=delta)


def _advance_month(cursor: datetime) -> datetime:
    if cursor.month == 12:
        return datetime(cursor.year + 1, 1, 1)
    return datetime(cursor.year, cursor.month + 1, 1)


def _iter_profile_paydates(
    profile: PayrollProfile,
    start: datetime,
    end_excl: datetime,
    *,
    apply_roll: bool,
):
    start = _midnight(start)
    end_excl = _midnight(end_excl)

    if end_excl <= start:
        return

    if profile.cadence is PayCadence.WEEKLY:
        current = _next_weekday_on_or_after(
            max(start, profile.anchor_date), profile.weekday
        )
        while current < end_excl:
            yield (
                _roll_weekend(current, profile.weekend_roll) if apply_roll else current
            )
            current += timedelta(days=7)
        return

    if profile.cadence is PayCadence.BIWEEKLY:
        current = _next_weekday_on_or_after(profile.anchor_date, profile.weekday)
        while current < start:
            current += timedelta(days=14)
        while current < end_excl:
            yield (
                _roll_weekend(current, profile.weekend_roll) if apply_roll else current
            )
            current += timedelta(days=14)
        return

    cursor = datetime(start.year, start.month, 1)

    if profile.cadence is PayCadence.SEMIMONTHLY:
        while cursor < end_excl:
            for day in profile.semimonthly_days:
                pay_date = _month_day(cursor.year, cursor.month, day)
                if apply_roll:
                    pay_date = _roll_weekend(pay_date, profile.weekend_roll)
                if start <= pay_date < end_excl:
                    yield pay_date
            cursor = _advance_month(cursor)
        return

    while cursor < end_excl:
        pay_date = _month_day(cursor.year, cursor.month, profile.monthly_day)
        if apply_roll:
            pay_date = _roll_weekend(pay_date, profile.weekend_roll)
        if start <= pay_date < end_excl:
            yield pay_date
        cursor = _advance_month(cursor)


def sample_payroll_profile(
    policy: Policy,
    seed: SeedSource,
    *,
    employer_acct: str,
) -> PayrollProfile:
    rng_factory = build_rng_factory(seed)
    rng = rng_factory.rng("employer_payroll_profile", employer_acct)

    cadence = rng.weighted_choice(
        [
            PayCadence.WEEKLY,
            PayCadence.BIWEEKLY,
            PayCadence.SEMIMONTHLY,
            PayCadence.MONTHLY,
        ],
        [
            float(policy.weekly_pay_weight),
            float(policy.biweekly_pay_weight),
            float(policy.semimonthly_pay_weight),
            float(policy.monthly_pay_weight),
        ],
    )

    weekday = int(policy.payroll_default_weekday)
    if cadence in {PayCadence.WEEKLY, PayCadence.BIWEEKLY} and rng.coin(0.25):
        weekday = 3 if weekday == 4 else 4

    anchor = _next_weekday_on_or_after(datetime(2025, 1, 1), weekday)

    if cadence is PayCadence.BIWEEKLY and rng.coin(0.5):
        anchor += timedelta(days=7)

    lag_max = max(0, int(policy.posting_lag_days_max))
    posting_lag_days = 0 if lag_max == 0 else rng.int(0, lag_max + 1)

    if cadence is PayCadence.SEMIMONTHLY:
        semimonthly_days = (1, 15) if rng.coin(0.35) else (15, 31)
        return PayrollProfile(
            cadence=cadence,
            anchor_date=anchor,
            weekday=weekday,
            semimonthly_days=semimonthly_days,
            posting_lag_days=posting_lag_days,
        )

    if cadence is PayCadence.MONTHLY:
        monthly_day = rng.choice([28, 30, 31])
        return PayrollProfile(
            cadence=cadence,
            anchor_date=anchor,
            weekday=weekday,
            monthly_day=int(monthly_day),
            posting_lag_days=posting_lag_days,
        )

    return PayrollProfile(
        cadence=cadence,
        anchor_date=anchor,
        weekday=weekday,
        posting_lag_days=posting_lag_days,
    )


def pay_periods_in_year(profile: PayrollProfile, year: int) -> int:
    if profile.cadence is PayCadence.SEMIMONTHLY:
        return 24
    if profile.cadence is PayCadence.MONTHLY:
        return 12

    start = datetime(year, 1, 1)
    end_excl = datetime(year + 1, 1, 1)
    count = sum(
        1 for _ in _iter_profile_paydates(profile, start, end_excl, apply_roll=False)
    )
    return max(1, count)


def paydates_for_profile(
    profile: PayrollProfile,
    *,
    start: datetime,
    end_excl: datetime,
) -> list[datetime]:
    return list(_iter_profile_paydates(profile, start, end_excl, apply_roll=True))


def paydates_for_window(
    state: State,
    *,
    window_start: datetime,
    window_end_excl: datetime,
) -> list[datetime]:
    active_start = max(_midnight(window_start), _midnight(state.start))
    active_end_excl = min(_midnight(window_end_excl), _midnight(state.end))

    if active_end_excl <= active_start:
        return []

    return paydates_for_profile(
        state.payroll,
        start=active_start,
        end_excl=active_end_excl,
    )


def initialize(
    policy: Policy,
    seed: SeedSource,
    *,
    person_id: str,
    start_date: datetime,
    employers: list[str],
    annual_salary_source: SalarySource,
) -> State:
    """
    Bootstraps the initial employment state for a person.
    """
    _require_employers(employers)

    rng_factory = build_rng_factory(seed)
    gen = rng_factory.rng("employment_init", person_id).gen

    employer = pick_one(gen, employers, name="employers")
    payroll = sample_payroll_profile(policy, seed, employer_acct=employer)

    job_start, job_end = sample_backdated_interval(
        gen,
        anchor_date=start_date,
        years_min=policy.job_tenure_min,
        years_max=policy.job_tenure_max,
    )

    annual_salary_raw = float(annual_salary_source())
    annual_salary = annual_salary_raw * sample_lognormal_multiplier(gen, sigma=0.03)

    return State(
        employer_acct=employer,
        payroll=payroll,
        start=job_start,
        end=job_end,
        annual_salary=annual_salary,
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
    Transitions a person to a new job, applying tenure, payroll cadence, and salary bumps.
    """
    _require_employers(employers)

    rng_factory = build_rng_factory(seed)

    adv_gen = rng_factory.rng(
        "employment_advance", person_id, str(prev.switch_index + 1)
    ).gen

    employer = pick_different(
        adv_gen,
        employers,
        prev.employer_acct,
        name="employers",
    )
    payroll = sample_payroll_profile(policy, seed, employer_acct=employer)

    start, end = sample_forward_interval(
        adv_gen,
        start=now,
        years_min=policy.job_tenure_min,
        years_max=policy.job_tenure_max,
    )

    current_annual_salary = prev.annual_salary * _compound_growth_salary(
        policy,
        rng_factory,
        person_id,
        prev.start,
        now,
    )

    bump = _job_switch_bump(policy, rng_factory, person_id, prev.switch_index + 1)
    new_annual_salary = current_annual_salary * (1.0 + bump)

    return State(
        employer_acct=employer,
        payroll=payroll,
        start=start,
        end=end,
        annual_salary=new_annual_salary,
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
    Calculates the exact paycheck amount for a given pay date.
    """
    rng_factory = build_rng_factory(seed)

    annual_amount = state.annual_salary * _compound_growth_salary(
        policy,
        rng_factory,
        person_id,
        state.start,
        pay_date,
    )

    periods = pay_periods_in_year(state.payroll, pay_date.year)
    return round(max(50.0, float(annual_amount) / float(periods)), 2)
