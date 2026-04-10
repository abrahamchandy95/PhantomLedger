from datetime import datetime

from common.random import Rng, RngFactory

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
from .state import Lease as State, RentSource, SeedSource


def _require_landlords(landlords: list[str]) -> None:
    if not landlords:
        raise ValueError("The landlords list must be non-empty.")


def _rent_real_raise_for_year(
    policy: Policy,
    rng_factory: RngFactory,
    key: str,
    year: int,
) -> float:
    """Internal helper to calculate the real rent raise for a specific year."""
    gen = rng_factory.rng("rent_real_raise", key, str(year)).gen
    return sample_normal_clamped(
        gen,
        mu=policy.rent_raise_mu,
        sigma=policy.rent_raise_sigma,
        floor=policy.rent_raise_floor,
    )


def _compound_growth_rent(
    policy: Policy,
    rng_factory: RngFactory,
    payer_acct: str,
    start: datetime,
    now: datetime,
) -> float:
    """Internal helper to compound rent growth over time."""
    return compound_growth(
        policy=policy,
        rng_factory=rng_factory,
        key=payer_acct,
        start=start,
        now=now,
        annual_raise_source=_rent_real_raise_for_year,
    )


# --- Public API ---


def initialize(
    policy: Policy,
    seed: SeedSource,
    rng: Rng,
    *,
    payer_acct: str,
    start_date: datetime,
    landlords: list[str],
    base_rent_source: RentSource,
) -> State:
    """
    Bootstraps the initial lease state for a person.
    """
    _require_landlords(landlords)

    # Note: validation is now automatically handled in Policy.__post_init__
    rng_factory = build_rng_factory(seed)
    gen = rng_factory.rng("lease_init", payer_acct).gen

    landlord = pick_one(gen, landlords, name="landlords")
    lease_start, lease_end = sample_backdated_interval(
        gen,
        anchor_date=start_date,
        years_min=policy.lease_tenure_min,
        years_max=policy.lease_tenure_max,
    )

    base_rent_raw = float(base_rent_source())
    base_rent = base_rent_raw * sample_lognormal_multiplier(gen, sigma=0.05)

    # Advance global RNG to maintain determinism
    _ = rng.float()

    return State(
        landlord_acct=landlord,
        start=lease_start,
        end=lease_end,
        base_rent=base_rent,
        move_index=0,
    )


def advance(
    policy: Policy,
    seed: SeedSource,
    rng: Rng,
    *,
    payer_acct: str,
    now: datetime,
    landlords: list[str],
    prev: State,
    reset_rent_source: RentSource,
) -> State:
    """
    Transitions a person to a new lease, pulling a fresh base rent from the source.
    """
    _require_landlords(landlords)

    rng_factory = build_rng_factory(seed)

    # Use the optimized O(1) vectorized selection
    landlord = pick_different(
        rng.gen,
        landlords,
        prev.landlord_acct,
        name="landlords",
    )

    start, end = sample_forward_interval(
        rng.gen,
        start=now,
        years_min=policy.lease_tenure_min,
        years_max=policy.lease_tenure_max,
    )

    base_rent_raw = float(reset_rent_source())
    gen = rng_factory.rng("lease_reset_rent", payer_acct, str(prev.move_index + 1)).gen
    base_rent = base_rent_raw * sample_lognormal_multiplier(gen, sigma=0.05)

    return State(
        landlord_acct=landlord,
        start=start,
        end=end,
        base_rent=base_rent,
        move_index=prev.move_index + 1,
    )


def calculate_rent(
    policy: Policy,
    seed: SeedSource,
    *,
    payer_acct: str,
    state: State,
    pay_date: datetime,
) -> float:
    """
    Calculates the exact rent for a given pay period, including compounded inflation/raises.
    """
    rng_factory = build_rng_factory(seed)
    amount = state.base_rent * _compound_growth_rent(
        policy,
        rng_factory,
        payer_acct,
        state.start,
        pay_date,
    )
    return round(max(1.0, float(amount)), 2)
