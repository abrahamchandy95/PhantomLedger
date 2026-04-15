from dataclasses import dataclass

from ..environment import LiquidityConstraints


@dataclass(frozen=True, slots=True)
class LiquiditySnapshot:
    days_since_payday: int
    paycheck_sensitivity: float
    available_cash: float
    baseline_cash: float
    fixed_monthly_burden: float


def liquidity_multiplier(
    policy: LiquidityConstraints,
    snapshot: LiquiditySnapshot,
) -> float:
    if not policy.enabled:
        return 1.0

    relief = 0.0
    if policy.relief_days > 0 and snapshot.days_since_payday <= policy.relief_days:
        relief = float(policy.relief_days - snapshot.days_since_payday) / float(
            policy.relief_days
        )

    stress_days = max(0, snapshot.days_since_payday - policy.stress_start_day)
    stress = min(1.0, float(stress_days) / float(max(1, policy.stress_ramp_days)))

    cycle_down = stress * (0.35 + 0.95 * snapshot.paycheck_sensitivity)
    cycle_up = relief * (0.06 + 0.10 * snapshot.paycheck_sensitivity)

    cycle_mult = 1.0 - cycle_down + cycle_up
    cycle_mult = min(1.05, max(float(policy.absolute_floor), cycle_mult))

    cash_ref = max(150.0, float(snapshot.baseline_cash))
    cash_ratio = max(0.0, min(2.0, float(snapshot.available_cash) / cash_ref))
    cash_mult = min(1.10, max(0.0, 0.10 + cash_ratio))

    burden_ratio = max(0.0, min(2.0, float(snapshot.fixed_monthly_burden) / cash_ref))
    burden_mult = max(0.30, 1.0 - 0.35 * burden_ratio)

    mult = cycle_mult * cash_mult * burden_mult
    return min(1.10, max(0.0, mult))
