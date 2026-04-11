from collections.abc import Sequence
from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

import numpy as np

from common.business_accounts import (
    brokerage_custody_account_id,
    business_operating_account_id,
)
from common.channels import (
    CARD_SETTLEMENT,
    CLIENT_ACH_CREDIT,
    INVESTMENT_INFLOW,
    OWNER_DRAW,
    PLATFORM_PAYOUT,
)
from common.math import as_int, lognormal_by_median
from common.persona_names import FREELANCER, HNW, SMALLBIZ
from common.random import derive_seed
from common.transactions import Transaction
from transfers.factory import TransactionDraft, TransactionFactory

from .models import LegitGenerationRequest
from .plans import LegitBuildPlan


@dataclass(frozen=True, slots=True)
class IncomeArchetype:
    client_active_p: float = 0.0
    client_counterparties_min: int = 1
    client_counterparties_max: int = 1
    client_payments_min: int = 0
    client_payments_max: int = 0
    client_median: float = 0.0
    client_sigma: float = 0.0

    platform_active_p: float = 0.0
    platform_counterparties_min: int = 1
    platform_counterparties_max: int = 1
    platform_payments_min: int = 0
    platform_payments_max: int = 0
    platform_median: float = 0.0
    platform_sigma: float = 0.0

    settlement_active_p: float = 0.0
    settlement_payments_min: int = 0
    settlement_payments_max: int = 0
    settlement_median: float = 0.0
    settlement_sigma: float = 0.0

    owner_draw_active_p: float = 0.0
    owner_draw_payments_min: int = 0
    owner_draw_payments_max: int = 0
    owner_draw_median: float = 0.0
    owner_draw_sigma: float = 0.0

    investment_active_p: float = 0.0
    investment_payments_min: int = 0
    investment_payments_max: int = 0
    investment_median: float = 0.0
    investment_sigma: float = 0.0

    quiet_month_p: float = 0.0


_ARCHETYPES: dict[str, IncomeArchetype] = {
    FREELANCER: IncomeArchetype(
        client_active_p=0.88,
        client_counterparties_min=2,
        client_counterparties_max=5,
        client_payments_min=1,
        client_payments_max=4,
        client_median=1400.0,
        client_sigma=0.70,
        platform_active_p=0.42,
        platform_counterparties_min=1,
        platform_counterparties_max=2,
        platform_payments_min=1,
        platform_payments_max=4,
        platform_median=425.0,
        platform_sigma=0.60,
        owner_draw_active_p=0.70,
        owner_draw_payments_min=1,
        owner_draw_payments_max=2,
        owner_draw_median=1800.0,
        owner_draw_sigma=0.75,
        quiet_month_p=0.12,
    ),
    SMALLBIZ: IncomeArchetype(
        client_active_p=0.55,
        client_counterparties_min=2,
        client_counterparties_max=6,
        client_payments_min=0,
        client_payments_max=3,
        client_median=2600.0,
        client_sigma=0.75,
        platform_active_p=0.22,
        platform_counterparties_min=1,
        platform_counterparties_max=2,
        platform_payments_min=0,
        platform_payments_max=3,
        platform_median=950.0,
        platform_sigma=0.70,
        settlement_active_p=0.74,
        settlement_payments_min=4,
        settlement_payments_max=12,
        settlement_median=680.0,
        settlement_sigma=0.55,
        owner_draw_active_p=0.86,
        owner_draw_payments_min=1,
        owner_draw_payments_max=2,
        owner_draw_median=3400.0,
        owner_draw_sigma=0.70,
        quiet_month_p=0.06,
    ),
    HNW: IncomeArchetype(
        investment_active_p=0.72,
        investment_payments_min=0,
        investment_payments_max=2,
        investment_median=6500.0,
        investment_sigma=1.05,
        quiet_month_p=0.02,
    ),
}


def _rand_int(
    gen: np.random.Generator,
    low: int,
    high_exclusive: int,
) -> int:
    return as_int(cast(int | np.integer, gen.integers(low, high_exclusive)))


def _pick_one(
    gen: np.random.Generator,
    items: Sequence[str],
) -> str | None:
    if not items:
        return None
    idx = _rand_int(gen, 0, len(items))
    return items[idx]


def _choice_k(
    gen: np.random.Generator,
    items: Sequence[str],
    *,
    low: int,
    high: int,
) -> list[str]:
    if not items:
        return []

    low = max(1, int(low))
    high = max(low, int(high))
    k = min(_rand_int(gen, low, high + 1), len(items))

    pool = list(items)
    out: list[str] = []

    for _ in range(k):
        idx = _rand_int(gen, 0, len(pool))
        out.append(pool.pop(idx))

    return out


def _business_day_ts(
    month_start: datetime,
    gen: np.random.Generator,
    *,
    earliest_hour: int,
    latest_hour: int,
    start_day: int = 0,
    end_day_exclusive: int = 28,
) -> datetime:
    start_day = max(0, min(27, int(start_day)))
    end_day_exclusive = max(start_day + 1, min(28, int(end_day_exclusive)))

    for _ in range(16):
        day = _rand_int(gen, start_day, end_day_exclusive)
        ts = month_start + timedelta(
            days=day,
            hours=_rand_int(gen, earliest_hour, latest_hour + 1),
            minutes=_rand_int(gen, 0, 60),
        )
        if ts.weekday() < 5:
            return ts

    ts = month_start + timedelta(days=_rand_int(gen, start_day, end_day_exclusive))
    while ts.weekday() >= 5:
        ts += timedelta(days=1)

    return ts.replace(
        hour=earliest_hour,
        minute=_rand_int(gen, 0, 60),
        second=0,
        microsecond=0,
    )


def _append_flow(
    txns: list[Transaction],
    txf: TransactionFactory,
    *,
    source: str,
    destination: str,
    amount: float,
    timestamp: datetime,
    channel: str,
    start_date: datetime,
    end_excl: datetime,
) -> None:
    if amount <= 0.0:
        return
    if timestamp < start_date or timestamp >= end_excl:
        return

    txns.append(
        txf.make(
            TransactionDraft(
                source=source,
                destination=destination,
                amount=round(amount, 2),
                timestamp=timestamp,
                channel=channel,
            )
        )
    )


def _payment_count(
    gen: np.random.Generator,
    *,
    low: int,
    high: int,
) -> int:
    return _rand_int(gen, low, high + 1)


def _owned_business_acct(
    accounts_by_person: dict[str, list[str]],
    person_id: str,
) -> str | None:
    expected = business_operating_account_id(person_id)
    person_accounts = accounts_by_person.get(person_id, [])
    if expected in person_accounts:
        return expected
    return None


def _owned_brokerage_acct(
    accounts_by_person: dict[str, list[str]],
    person_id: str,
) -> str | None:
    expected = brokerage_custody_account_id(person_id)
    person_accounts = accounts_by_person.get(person_id, [])
    if expected in person_accounts:
        return expected
    return None


def _client_credits(
    *,
    archetype: IncomeArchetype,
    month_gen: np.random.Generator,
    month_start: datetime,
    window_start: datetime,
    end_excl: datetime,
    dst_acct: str,
    client_sources: Sequence[str],
    txf: TransactionFactory,
    txns: list[Transaction],
) -> None:
    if not client_sources:
        return

    n = _payment_count(
        month_gen,
        low=archetype.client_payments_min,
        high=archetype.client_payments_max,
    )
    for _ in range(n):
        src = _pick_one(month_gen, client_sources)
        if src is None:
            continue

        amount = float(
            lognormal_by_median(
                month_gen,
                median=archetype.client_median,
                sigma=archetype.client_sigma,
            )
        )
        _append_flow(
            txns,
            txf,
            source=src,
            destination=dst_acct,
            amount=max(75.0, amount),
            timestamp=_business_day_ts(
                month_start,
                month_gen,
                earliest_hour=8,
                latest_hour=17,
                start_day=0,
                end_day_exclusive=28,
            ),
            channel=CLIENT_ACH_CREDIT,
            start_date=window_start,
            end_excl=end_excl,
        )


def _platform_payouts(
    *,
    archetype: IncomeArchetype,
    month_gen: np.random.Generator,
    month_start: datetime,
    window_start: datetime,
    end_excl: datetime,
    dst_acct: str,
    platform_sources: Sequence[str],
    txf: TransactionFactory,
    txns: list[Transaction],
) -> None:
    if not platform_sources:
        return

    n = _payment_count(
        month_gen,
        low=archetype.platform_payments_min,
        high=archetype.platform_payments_max,
    )
    for _ in range(n):
        src = _pick_one(month_gen, platform_sources)
        if src is None:
            continue

        amount = float(
            lognormal_by_median(
                month_gen,
                median=archetype.platform_median,
                sigma=archetype.platform_sigma,
            )
        )
        _append_flow(
            txns,
            txf,
            source=src,
            destination=dst_acct,
            amount=max(25.0, amount),
            timestamp=_business_day_ts(
                month_start,
                month_gen,
                earliest_hour=6,
                latest_hour=11,
                start_day=0,
                end_day_exclusive=28,
            ),
            channel=PLATFORM_PAYOUT,
            start_date=window_start,
            end_excl=end_excl,
        )


def _card_settlements(
    *,
    archetype: IncomeArchetype,
    month_gen: np.random.Generator,
    month_start: datetime,
    window_start: datetime,
    end_excl: datetime,
    dst_acct: str,
    processor_source: str | None,
    txf: TransactionFactory,
    txns: list[Transaction],
) -> None:
    if processor_source is None:
        return

    n = _payment_count(
        month_gen,
        low=archetype.settlement_payments_min,
        high=archetype.settlement_payments_max,
    )
    for _ in range(n):
        amount = float(
            lognormal_by_median(
                month_gen,
                median=archetype.settlement_median,
                sigma=archetype.settlement_sigma,
            )
        )
        _append_flow(
            txns,
            txf,
            source=processor_source,
            destination=dst_acct,
            amount=max(20.0, amount),
            timestamp=_business_day_ts(
                month_start,
                month_gen,
                earliest_hour=5,
                latest_hour=9,
                start_day=0,
                end_day_exclusive=28,
            ),
            channel=CARD_SETTLEMENT,
            start_date=window_start,
            end_excl=end_excl,
        )


def _owner_draws(
    *,
    archetype: IncomeArchetype,
    month_gen: np.random.Generator,
    month_start: datetime,
    window_start: datetime,
    end_excl: datetime,
    src_acct: str | None,
    dst_acct: str,
    txf: TransactionFactory,
    txns: list[Transaction],
) -> None:
    if src_acct is None:
        return

    n = _payment_count(
        month_gen,
        low=archetype.owner_draw_payments_min,
        high=archetype.owner_draw_payments_max,
    )
    for _ in range(n):
        amount = float(
            lognormal_by_median(
                month_gen,
                median=archetype.owner_draw_median,
                sigma=archetype.owner_draw_sigma,
            )
        )
        _append_flow(
            txns,
            txf,
            source=src_acct,
            destination=dst_acct,
            amount=max(100.0, amount),
            timestamp=_business_day_ts(
                month_start,
                month_gen,
                earliest_hour=10,
                latest_hour=17,
                start_day=8,
                end_day_exclusive=28,
            ),
            channel=OWNER_DRAW,
            start_date=window_start,
            end_excl=end_excl,
        )


def _investment_inflows(
    *,
    archetype: IncomeArchetype,
    month_gen: np.random.Generator,
    month_start: datetime,
    window_start: datetime,
    end_excl: datetime,
    src_acct: str | None,
    dst_acct: str,
    txf: TransactionFactory,
    txns: list[Transaction],
) -> None:
    if src_acct is None:
        return

    n = _payment_count(
        month_gen,
        low=archetype.investment_payments_min,
        high=archetype.investment_payments_max,
    )
    for _ in range(n):
        amount = float(
            lognormal_by_median(
                month_gen,
                median=archetype.investment_median,
                sigma=archetype.investment_sigma,
            )
        )
        _append_flow(
            txns,
            txf,
            source=src_acct,
            destination=dst_acct,
            amount=max(250.0, amount),
            timestamp=_business_day_ts(
                month_start,
                month_gen,
                earliest_hour=7,
                latest_hour=15,
                start_day=0,
                end_day_exclusive=28,
            ),
            channel=INVESTMENT_INFLOW,
            start_date=window_start,
            end_excl=end_excl,
        )


def generate_nonpayroll_income_txns(
    request: LegitGenerationRequest,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
) -> list[Transaction]:
    pools = request.overrides.counterparty_pools
    if pools is None:
        return []

    end_excl = plan.start_date + timedelta(days=plan.days)
    txns: list[Transaction] = []
    accounts_by_person = request.inputs.accounts.by_person

    for person_id in plan.persons:
        persona = plan.personas.persona_for_person.get(person_id)
        archetype = _ARCHETYPES.get(persona or "")
        if archetype is None:
            continue

        personal_acct = plan.primary_acct_for_person.get(person_id)
        if personal_acct is None:
            continue
        if personal_acct in plan.counterparties.hub_set:
            continue

        business_acct = _owned_business_acct(accounts_by_person, person_id)
        brokerage_acct = _owned_brokerage_acct(accounts_by_person, person_id)

        revenue_dst_acct = (
            business_acct
            if persona in {FREELANCER, SMALLBIZ} and business_acct is not None
            else personal_acct
        )

        person_gen = np.random.default_rng(
            derive_seed(plan.seed, "legit", "nonpayroll_income", person_id)
        )

        client_sources = (
            _choice_k(
                person_gen,
                pools.client_payer_ids,
                low=archetype.client_counterparties_min,
                high=archetype.client_counterparties_max,
            )
            if archetype.client_active_p > 0.0
            and person_gen.random() < archetype.client_active_p
            else []
        )

        platform_sources = (
            _choice_k(
                person_gen,
                pools.platform_ids,
                low=archetype.platform_counterparties_min,
                high=archetype.platform_counterparties_max,
            )
            if archetype.platform_active_p > 0.0
            and person_gen.random() < archetype.platform_active_p
            else []
        )

        processor_source = (
            _pick_one(person_gen, pools.processor_ids)
            if archetype.settlement_active_p > 0.0
            and pools.processor_ids
            and person_gen.random() < archetype.settlement_active_p
            else None
        )

        fallback_owner_business_source = (
            _pick_one(person_gen, pools.owner_business_ids)
            if business_acct is None
            and archetype.owner_draw_active_p > 0.0
            and pools.owner_business_ids
            and person_gen.random() < archetype.owner_draw_active_p
            else None
        )

        investment_source = None
        if brokerage_acct is not None:
            investment_source = brokerage_acct
        elif (
            archetype.investment_active_p > 0.0
            and pools.brokerage_ids
            and person_gen.random() < archetype.investment_active_p
        ):
            investment_source = _pick_one(person_gen, pools.brokerage_ids)

        if not any(
            (
                client_sources,
                platform_sources,
                processor_source,
                business_acct,
                fallback_owner_business_source,
                investment_source,
            )
        ):
            if persona == FREELANCER and pools.client_payer_ids:
                client_sources = _choice_k(
                    person_gen,
                    pools.client_payer_ids,
                    low=1,
                    high=2,
                )
            elif (
                persona == SMALLBIZ
                and business_acct is None
                and pools.owner_business_ids
            ):
                fallback_owner_business_source = _pick_one(
                    person_gen,
                    pools.owner_business_ids,
                )
            elif persona == HNW:
                if brokerage_acct is not None:
                    investment_source = brokerage_acct
                elif pools.brokerage_ids:
                    investment_source = _pick_one(person_gen, pools.brokerage_ids)

        for month_start in plan.month_starts:
            month_gen = np.random.default_rng(
                derive_seed(
                    plan.seed,
                    "legit",
                    "nonpayroll_income",
                    person_id,
                    str(month_start.year),
                    str(month_start.month),
                )
            )

            if (
                month_gen.random() < archetype.quiet_month_p
                and month_gen.random() < 0.60
            ):
                continue

            _client_credits(
                archetype=archetype,
                month_gen=month_gen,
                month_start=month_start,
                window_start=plan.start_date,
                end_excl=end_excl,
                dst_acct=revenue_dst_acct,
                client_sources=client_sources,
                txf=txf,
                txns=txns,
            )

            _platform_payouts(
                archetype=archetype,
                month_gen=month_gen,
                month_start=month_start,
                window_start=plan.start_date,
                end_excl=end_excl,
                dst_acct=revenue_dst_acct,
                platform_sources=platform_sources,
                txf=txf,
                txns=txns,
            )

            _card_settlements(
                archetype=archetype,
                month_gen=month_gen,
                month_start=month_start,
                window_start=plan.start_date,
                end_excl=end_excl,
                dst_acct=revenue_dst_acct,
                processor_source=processor_source,
                txf=txf,
                txns=txns,
            )

            _owner_draws(
                archetype=archetype,
                month_gen=month_gen,
                month_start=month_start,
                window_start=plan.start_date,
                end_excl=end_excl,
                src_acct=business_acct or fallback_owner_business_source,
                dst_acct=personal_acct,
                txf=txf,
                txns=txns,
            )

            _investment_inflows(
                archetype=archetype,
                month_gen=month_gen,
                month_start=month_start,
                window_start=plan.start_date,
                end_excl=end_excl,
                src_acct=investment_source,
                dst_acct=personal_acct,
                txf=txf,
                txns=txns,
            )

    txns.sort(
        key=lambda txn: (
            txn.timestamp,
            txn.source,
            txn.target,
            float(txn.amount),
            txn.channel or "",
        )
    )
    return txns
