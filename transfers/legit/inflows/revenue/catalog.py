from common.persona_names import FREELANCER, HNW, SMALLBIZ

from .profiles import (
    CounterpartyRevenueProfile,
    QuietMonthPolicy,
    RevenueFlowProfile,
    RevenuePersonaProfile,
)


REVENUE_ARCHETYPES: dict[str, RevenuePersonaProfile] = {
    FREELANCER: RevenuePersonaProfile(
        client=CounterpartyRevenueProfile(
            active_p=0.88,
            counterparties_min=2,
            counterparties_max=5,
            payments_min=1,
            payments_max=4,
            median=1400.0,
            sigma=0.70,
        ),
        platform=CounterpartyRevenueProfile(
            active_p=0.42,
            counterparties_min=1,
            counterparties_max=2,
            payments_min=1,
            payments_max=4,
            median=425.0,
            sigma=0.60,
        ),
        owner_draw=RevenueFlowProfile(
            active_p=0.70,
            payments_min=1,
            payments_max=2,
            median=1800.0,
            sigma=0.75,
        ),
        quiet_month=QuietMonthPolicy(probability=0.12),
    ),
    SMALLBIZ: RevenuePersonaProfile(
        client=CounterpartyRevenueProfile(
            active_p=0.55,
            counterparties_min=2,
            counterparties_max=6,
            payments_min=0,
            payments_max=3,
            median=2600.0,
            sigma=0.75,
        ),
        platform=CounterpartyRevenueProfile(
            active_p=0.22,
            counterparties_min=1,
            counterparties_max=2,
            payments_min=0,
            payments_max=3,
            median=950.0,
            sigma=0.70,
        ),
        settlement=RevenueFlowProfile(
            active_p=0.74,
            payments_min=4,
            payments_max=12,
            median=680.0,
            sigma=0.55,
        ),
        owner_draw=RevenueFlowProfile(
            active_p=0.86,
            payments_min=1,
            payments_max=2,
            median=3400.0,
            sigma=0.70,
        ),
        quiet_month=QuietMonthPolicy(probability=0.06),
    ),
    HNW: RevenuePersonaProfile(
        investment=RevenueFlowProfile(
            active_p=0.72,
            payments_min=0,
            payments_max=2,
            median=6500.0,
            sigma=1.05,
        ),
        quiet_month=QuietMonthPolicy(probability=0.02),
    ),
}
