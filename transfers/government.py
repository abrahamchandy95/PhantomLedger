from collections.abc import Sequence
from datetime import date, datetime, time, timedelta
from typing import cast

from common.channels import GOV_SOCIAL_SECURITY, DISABILITY
from common.persona_names import STUDENT, RETIRED
from common.config.population.government import Government
from common.math import lognormal_by_median
from common.random import Rng
from common.transactions import Transaction
from common.timeline import active_months
from common.config import Window
from common.externals import GOV_SSA, GOV_DISABILITY
from transfers.factory import TransactionDraft, TransactionFactory


def _at_midnight(d: date) -> datetime:
    if isinstance(d, datetime):
        return d.replace(hour=0, minute=0, second=0, microsecond=0)
    return datetime.combine(d, time.min)


def generate(
    cfg: Government,
    window: Window,
    rng: Rng,
    txf: TransactionFactory,
    *,
    personas: dict[str, str],
    primary_accounts: dict[str, str],
) -> list[Transaction]:
    """Generates government benefit deposits."""
    txns: list[Transaction] = []

    start = window.start_date
    days = int(window.days)
    months = cast(list[date], active_months(start, days))
    end_excl: datetime = _at_midnight(start) + timedelta(days=days)

    if not months:
        return txns

    if cfg.ss_enabled:
        txns.extend(
            _social_security(
                cfg, rng, txf, months, end_excl, personas, primary_accounts
            )
        )

    if cfg.disability_enabled:
        txns.extend(
            _disability(cfg, rng, txf, months, end_excl, personas, primary_accounts)
        )

    return txns


def _social_security(
    cfg: Government,
    rng: Rng,
    txf: TransactionFactory,
    months: Sequence[date],
    end_excl: datetime,
    personas: dict[str, str],
    primary_accounts: dict[str, str],
) -> list[Transaction]:
    txns: list[Transaction] = []

    retirees = [pid for pid, persona in personas.items() if persona == "retired"]

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

    for month_start in months:
        month_base = _at_midnight(month_start)

        for pid, amount in recipients.items():
            acct = primary_accounts[pid]

            ts: datetime = month_base + timedelta(
                days=rng.int(8, 23),
                hours=rng.int(6, 10),
                minutes=rng.int(0, 60),
            )
            if ts >= end_excl:
                continue

            txns.append(
                txf.make(
                    TransactionDraft(
                        source=GOV_SSA,
                        destination=acct,
                        amount=amount,
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
    months: Sequence[date],
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

    for month_start in months:
        month_base = _at_midnight(month_start)

        for pid, amount in recipients.items():
            acct = primary_accounts[pid]

            ts: datetime = month_base + timedelta(
                days=rng.int(1, 15),
                hours=rng.int(6, 10),
                minutes=rng.int(0, 60),
            )
            if ts >= end_excl:
                continue

            txns.append(
                txf.make(
                    TransactionDraft(
                        source=GOV_DISABILITY,
                        destination=acct,
                        amount=amount,
                        timestamp=ts,
                        channel=DISABILITY,
                    )
                )
            )

    return txns
