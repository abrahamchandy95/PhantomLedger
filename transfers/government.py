from datetime import date, datetime, time, timedelta
from hashlib import blake2b

from common.calendar_cache import WindowCalendar, ssa_bucket_for_birth_day
from common.channels import DISABILITY, GOV_SOCIAL_SECURITY
from common.config import Window
from common.config.population.government import Government
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


def _ssa_recipient_rows(
    recipients: dict[str, float],
    primary_accounts: dict[str, str],
) -> list[tuple[str, str, float, int]]:
    rows: list[tuple[str, str, float, int]] = []

    for pid in sorted(recipients):
        rows.append(
            (
                pid,
                primary_accounts[pid],
                recipients[pid],
                ssa_bucket_for_birth_day(_synthetic_birth_day(pid)),
            )
        )

    return rows


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
    calendar = WindowCalendar(start, end_excl)

    if not calendar.month_anchors:
        return txns

    if cfg.ss_enabled:
        txns.extend(
            _social_security(
                cfg,
                rng,
                txf,
                calendar,
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
                calendar,
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
    calendar: WindowCalendar,
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

    rows = _ssa_recipient_rows(recipients, primary_accounts)
    if not rows:
        return txns

    pay_dates_by_bucket = {
        0: calendar.ssa_payment_dates_for_bucket(0),
        1: calendar.ssa_payment_dates_for_bucket(1),
        2: calendar.ssa_payment_dates_for_bucket(2),
    }

    for month_idx in range(len(calendar.month_anchors)):
        for pid, acct, amount, bucket in rows:
            pay_date = pay_dates_by_bucket[bucket][month_idx]
            ts = _payment_timestamp(rng, pay_date)

            if ts < start or ts >= end_excl:
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
    calendar: WindowCalendar,
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

    rows = _ssa_recipient_rows(recipients, primary_accounts)
    if not rows:
        return txns

    pay_dates_by_bucket = {
        0: calendar.ssa_payment_dates_for_bucket(0),
        1: calendar.ssa_payment_dates_for_bucket(1),
        2: calendar.ssa_payment_dates_for_bucket(2),
    }

    for month_idx in range(len(calendar.month_anchors)):
        for pid, acct, amount, bucket in rows:
            pay_date = pay_dates_by_bucket[bucket][month_idx]
            ts = _payment_timestamp(rng, pay_date)

            if ts < start or ts >= end_excl:
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
