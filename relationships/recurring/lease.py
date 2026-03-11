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
from .models import LeaseState
from .policy import RecurringPolicy
from .types import RentSampler, SeedSource


def _require_landlords(landlords: list[str]) -> None:
    if not landlords:
        raise ValueError("landlords must be non-empty")


def rent_real_raise_for_year(
    policy: RecurringPolicy,
    seedbank: SeedBank,
    key: str,
    year: int,
) -> float:
    gen = seedbank.generator("rent_real_raise", key, str(year))
    return normal_clamped(
        gen,
        mu=policy.rent_real_raise_mu,
        sigma=policy.rent_real_raise_sigma,
        floor=policy.rent_real_raise_floor,
    )


def compound_growth_rent(
    policy: RecurringPolicy,
    seedbank: SeedBank,
    payer_acct: str,
    start: datetime,
    now: datetime,
) -> float:
    return compound_growth(
        policy=policy,
        seedbank=seedbank,
        key=payer_acct,
        start=start,
        now=now,
        annual_real_raise=rent_real_raise_for_year,
    )


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
    _require_landlords(landlords)

    seedbank = validated_seedbank(policy, seed)
    gen = seedbank.generator("lease_init", payer_acct)

    landlord = pick_one(gen, landlords, name="landlords")
    lease_start, lease_end = sample_backdated_interval(
        gen,
        anchor_date=start_date,
        years_min=policy.landlord_tenure_years_min,
        years_max=policy.landlord_tenure_years_max,
    )

    base_rent_raw = float(base_rent_sampler())
    base_rent = base_rent_raw * lognormal_multiplier(gen, sigma=0.05)

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
    _require_landlords(landlords)

    seedbank = as_seedbank(seed)

    landlord = pick_different_or_same(
        rng,
        landlords,
        prev.landlord_acct,
        name="landlords",
    )

    start, end = sample_forward_interval(
        rng.gen,
        start=now,
        years_min=policy.landlord_tenure_years_min,
        years_max=policy.landlord_tenure_years_max,
    )

    base_rent_raw = float(reset_rent_sampler())
    gen = seedbank.generator("lease_reset_rent", payer_acct, str(prev.move_index + 1))
    base_rent = base_rent_raw * lognormal_multiplier(gen, sigma=0.05)

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
    seedbank = as_seedbank(seed)
    amount = state.base_rent * compound_growth_rent(
        policy,
        seedbank,
        payer_acct,
        state.start,
        pay_date,
    )
    return round(max(1.0, float(amount)), 2)
