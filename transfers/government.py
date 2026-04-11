import calendar
from collections.abc import Sequence
from datetime import date, datetime, time, timedelta
from hashlib import blake2b

from common.channels import DISABILITY, GOV_SOCIAL_SECURITY
from common.config import Window
from common.config.population.government import Government
from common.date_math import month_starts
from common.externals import GOV_DISABILITY, GOV_SSA
from common.math import lognormal_by_median
from common.persona_names import RETIRED, STUDENT
from common.random import Rng
from common.transactions import Transaction
from transfers.factory import TransactionDraft, TransactionFactory


def _at_midnight(d: date) -> datetime:
    if isinstance(d, datetime):
        return d.replace(hour=0, minute=0, second=0, microsecond=0)
    return datetime.combine(d, time.min)


def _stable_u64(*parts: str) -> int:
    """
    Deterministically derive a 64-bit integer from string parts.

    This mirrors the hashing scheme used by export/mule_ml/party.py so that
    the synthetic payment cohort stays aligned with the exported fake DOB.
    """
    h = blake2b(digest_size=8)
    for part in parts:
        h.update(part.encode("utf-8"))
        h.update(b"|")
    return int.from_bytes(h.digest(), "little", signed=False)


def _synthetic_birth_day(person_id: str) -> int:
    """
    Return a stable synthetic day-of-month for SSA payment cohorting.

    The repo's export/mule_ml/party.py generator derives fake DOBs with:
        day = 1 + ((hash >> 16) % 28)

    Reusing the same mapping here means the Wednesday cohort assignment is
    consistent with the DOB that downstream ML exports expose, even though the
    transfer generator itself does not receive a full DOB field.
    """
    x = _stable_u64(person_id, "identity")
    return 1 + ((x >> 16) % 28)


def _nth_weekday_of_month(
    year: int,
    month: int,
    *,
    weekday: int,
    occurrence: int,
) -> date:
    if occurrence <= 0:
        raise ValueError(f"occurrence must be >= 1, got {occurrence}")

    first = date(year, month, 1)
    delta = (weekday - first.weekday()) % 7
    candidate = first + timedelta(days=delta + 7 * (occurrence - 1))

    if candidate.month != month:
        raise ValueError(
            f"{occurrence} occurrence of weekday {weekday} does not exist in "
            + f"{year:04d}-{month:02d}"
        )

    return candidate


def _last_weekday_of_month(year: int, month: int, *, weekday: int) -> date:
    last_dom = calendar.monthrange(year, month)[1]
    last = date(year, month, last_dom)
    delta = (last.weekday() - weekday) % 7
    return last - timedelta(days=delta)


def _observed_fixed_holiday(year: int, month: int, day: int) -> date:
    actual = date(year, month, day)
    if actual.weekday() == 5:
        return actual - timedelta(days=1)
    if actual.weekday() == 6:
        return actual + timedelta(days=1)
    return actual


def _us_federal_holidays(year: int) -> set[date]:
    """
    Federal legal holidays needed for SSA payment rollback.

    SSA states that if a scheduled Wednesday payment date is a federal legal
    holiday, payment is made on the preceding day that is not a federal holiday.
    """
    return {
        _observed_fixed_holiday(year, 1, 1),  # New Year's Day
        _nth_weekday_of_month(year, 1, weekday=0, occurrence=3),  # MLK Day
        _nth_weekday_of_month(
            year, 2, weekday=0, occurrence=3
        ),  # Washington's Birthday
        _last_weekday_of_month(year, 5, weekday=0),  # Memorial Day
        _observed_fixed_holiday(year, 6, 19),  # Juneteenth
        _observed_fixed_holiday(year, 7, 4),  # Independence Day
        _nth_weekday_of_month(year, 9, weekday=0, occurrence=1),  # Labor Day
        _nth_weekday_of_month(year, 10, weekday=0, occurrence=2),  # Columbus Day
        _observed_fixed_holiday(year, 11, 11),  # Veterans Day
        _nth_weekday_of_month(year, 11, weekday=3, occurrence=4),  # Thanksgiving
        _observed_fixed_holiday(year, 12, 25),  # Christmas Day
    }


def _is_business_day(ts: date, holiday_cache: dict[int, set[date]]) -> bool:
    if ts.weekday() >= 5:
        return False

    holidays = holiday_cache.setdefault(ts.year, _us_federal_holidays(ts.year))
    return ts not in holidays


def _ssa_cycle_payment_date(
    year: int,
    month: int,
    *,
    birth_day: int,
    holiday_cache: dict[int, set[date]],
) -> date:
    """
    Standard SSA/RSDI cohorting for post-1997 cycle payments.

    Official rule:
    - 1st-10th  -> second Wednesday
    - 11th-20th -> third Wednesday
    - 21st-31st -> fourth Wednesday

    If the scheduled Wednesday is a federal holiday, SSA pays on the preceding
    day that is not a federal holiday. We also keep the result on a business day.
    """
    if birth_day <= 10:
        occurrence = 2
    elif birth_day <= 20:
        occurrence = 3
    else:
        occurrence = 4

    pay_date = _nth_weekday_of_month(year, month, weekday=2, occurrence=occurrence)

    while not _is_business_day(pay_date, holiday_cache):
        pay_date -= timedelta(days=1)

    return pay_date


def _payment_timestamp(
    rng: Rng,
    payment_date: date,
) -> datetime:
    return _at_midnight(payment_date) + timedelta(
        hours=rng.int(6, 10),
        minutes=rng.int(0, 60),
    )


def generate(
    cfg: Government,
    window: Window,
    rng: Rng,
    txf: TransactionFactory,
    *,
    personas: dict[str, str],
    primary_accounts: dict[str, str],
) -> list[Transaction]:
    """Generate government benefit deposits."""
    txns: list[Transaction] = []

    start = window.start_date
    end_excl = _at_midnight(start) + timedelta(days=int(window.days))

    # Cover every month touched by the simulation window so a run that starts
    # mid-month can still include that month's scheduled benefit day.
    months = list(month_starts(start, end_excl))

    if not months:
        return txns

    if cfg.ss_enabled:
        txns.extend(
            _social_security(
                cfg,
                rng,
                txf,
                months,
                start,
                end_excl,
                personas,
                primary_accounts,
            )
        )

    if cfg.disability_enabled:
        txns.extend(
            _disability(
                cfg,
                rng,
                txf,
                months,
                start,
                end_excl,
                personas,
                primary_accounts,
            )
        )

    return txns


def _social_security(
    cfg: Government,
    rng: Rng,
    txf: TransactionFactory,
    months: Sequence[datetime],
    start: datetime,
    end_excl: datetime,
    personas: dict[str, str],
    primary_accounts: dict[str, str],
) -> list[Transaction]:
    txns: list[Transaction] = []

    retirees = [pid for pid, persona in personas.items() if persona == RETIRED]

    recipients: dict[str, float] = {}
    for pid in retirees:
        if not rng.coin(cfg.ss_eligible_p):
            continue

        acct = primary_accounts.get(pid)
        if not acct:
            continue

        amount = lognormal_by_median(
            rng.gen,
            median=cfg.ss_median,
            sigma=cfg.ss_sigma,
        )
        recipients[pid] = round(max(cfg.ss_floor, float(amount)), 2)

    holiday_cache: dict[int, set[date]] = {}

    for month_start in months:
        for pid in sorted(recipients):
            acct = primary_accounts[pid]
            birth_day = _synthetic_birth_day(pid)

            pay_date = _ssa_cycle_payment_date(
                month_start.year,
                month_start.month,
                birth_day=birth_day,
                holiday_cache=holiday_cache,
            )
            ts = _payment_timestamp(rng, pay_date)

            if ts < start or ts >= end_excl:
                continue

            txns.append(
                txf.make(
                    TransactionDraft(
                        source=GOV_SSA,
                        destination=acct,
                        amount=recipients[pid],
                        timestamp=ts,
                        channel=GOV_SOCIAL_SECURITY,
                    )
                )
            )

    return txns


def _disability(
    cfg: Government,
    rng: Rng,
    txf: TransactionFactory,
    months: Sequence[datetime],
    start: datetime,
    end_excl: datetime,
    personas: dict[str, str],
    primary_accounts: dict[str, str],
) -> list[Transaction]:
    txns: list[Transaction] = []

    eligible = [
        pid for pid, persona in personas.items() if persona not in (RETIRED, STUDENT)
    ]

    recipients: dict[str, float] = {}
    for pid in eligible:
        if not rng.coin(cfg.disability_p):
            continue

        acct = primary_accounts.get(pid)
        if not acct:
            continue

        amount = lognormal_by_median(
            rng.gen,
            median=cfg.disability_median,
            sigma=cfg.disability_sigma,
        )
        recipients[pid] = round(max(cfg.disability_floor, float(amount)), 2)

    holiday_cache: dict[int, set[date]] = {}

    for month_start in months:
        for pid in sorted(recipients):
            acct = primary_accounts[pid]
            birth_day = _synthetic_birth_day(pid)

            # SSDI is part of SSA's RSDI payment machinery, so the post-1997
            # birthday-cycle Wednesday rule is a better default than a random
            # day in the first half of the month.
            pay_date = _ssa_cycle_payment_date(
                month_start.year,
                month_start.month,
                birth_day=birth_day,
                holiday_cache=holiday_cache,
            )
            ts = _payment_timestamp(rng, pay_date)

            if ts < start or ts >= end_excl:
                continue

            txns.append(
                txf.make(
                    TransactionDraft(
                        source=GOV_DISABILITY,
                        destination=acct,
                        amount=recipients[pid],
                        timestamp=ts,
                        channel=DISABILITY,
                    )
                )
            )

    return txns
