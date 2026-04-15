"""
Intra-person account transfers.

"""

from collections.abc import Sequence
from dataclasses import dataclass
from datetime import datetime, timedelta

from common.channels import SELF_TRANSFER
from common.math import lognormal_by_median
from common.random import Rng
from common.transactions import Transaction
from common.validate import between, ge, gt
from transfers.balances import ClearingHouse
from transfers.factory import TransactionDraft, TransactionFactory
from transfers.screening import advance_book_through

from transfers.legit.blueprints import LegitBuildPlan


@dataclass(frozen=True, slots=True)
class SelfTransferConfig:
    active_p: float = 0.45

    transfers_per_month_min: int = 1
    transfers_per_month_max: int = 3

    amount_median: float = 250.0
    amount_sigma: float = 0.8

    round_amount_p: float = 0.45

    def __post_init__(self) -> None:
        between("active_p", self.active_p, 0.0, 1.0)
        ge("transfers_per_month_min", self.transfers_per_month_min, 1)
        ge(
            "transfers_per_month_max",
            self.transfers_per_month_max,
            self.transfers_per_month_min,
        )
        gt("amount_median", self.amount_median, 0.0)
        ge("amount_sigma", self.amount_sigma, 0.0)
        between("round_amount_p", self.round_amount_p, 0.0, 1.0)


DEFAULT_SELF_TRANSFER_CONFIG = SelfTransferConfig()

_ROUND_AMOUNTS: tuple[float, ...] = (
    25.0,
    50.0,
    50.0,
    100.0,
    100.0,
    100.0,
    150.0,
    200.0,
    200.0,
    250.0,
    300.0,
    500.0,
    500.0,
    750.0,
    1000.0,
    1000.0,
    1500.0,
    2000.0,
)


def _choose_source_account(
    rng: Rng,
    eligible: list[str],
    amount: float,
    book: ClearingHouse | None,
) -> str | None:
    if not eligible:
        return None

    if book is None:
        return rng.choice(eligible)

    ranked = sorted(
        ((book.available_cash(acct), acct) for acct in eligible),
        reverse=True,
    )

    for available_cash, acct in ranked:
        if available_cash >= amount:
            return acct

    return None


def _choose_destination_account(
    rng: Rng,
    source: str,
    eligible: list[str],
    book: ClearingHouse | None,
) -> str | None:
    destinations = [acct for acct in eligible if acct != source]
    if not destinations:
        return None

    if book is None or len(destinations) == 1:
        return rng.choice(destinations)

    # Prefer topping up the relatively leaner cash account.
    ranked = sorted(destinations, key=lambda acct: book.available_cash(acct))
    return ranked[0]


def generate(
    rng: Rng,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
    accounts_by_person: dict[str, list[str]],
    cfg: SelfTransferConfig = DEFAULT_SELF_TRANSFER_CONFIG,
    *,
    book: ClearingHouse | None = None,
    base_txns: Sequence[Transaction] | None = None,
    base_txns_sorted: bool = False,
) -> list[Transaction]:
    if not plan.paydays:
        return []

    end_excl = plan.start_date + timedelta(days=plan.days)
    hub_set = plan.counterparties.hub_set
    txns: list[Transaction] = []

    if base_txns is None:
        seeded: Sequence[Transaction] = ()
    elif base_txns_sorted:
        seeded = base_txns
    else:
        seeded = sorted(base_txns, key=lambda t: t.timestamp)
    seed_idx = 0

    active_people: list[tuple[str, list[str]]] = []
    for person_id in plan.persons:
        accts = accounts_by_person.get(person_id, [])
        eligible = [a for a in accts if a not in hub_set]
        if len(eligible) < 2:
            continue
        if not rng.coin(cfg.active_p):
            continue
        active_people.append((person_id, eligible))

    for month_start in plan.paydays:
        month_candidates: list[tuple[datetime, list[str], float]] = []

        for _, eligible in active_people:
            n_transfers = rng.int(
                cfg.transfers_per_month_min,
                cfg.transfers_per_month_max + 1,
            )

            for _ in range(n_transfers):
                if rng.coin(cfg.round_amount_p):
                    amount = float(rng.choice(_ROUND_AMOUNTS))
                else:
                    amount = float(
                        lognormal_by_median(
                            rng.gen,
                            median=cfg.amount_median,
                            sigma=cfg.amount_sigma,
                        )
                    )
                    amount = round(max(10.0, amount), 2)

                # Bias toward shortly after the monthly deposit cadence.
                if rng.float() < 0.70:
                    day_offset = rng.int(0, 6)
                else:
                    day_offset = rng.int(6, 28)

                ts = month_start + timedelta(
                    days=day_offset,
                    hours=rng.int(8, 22),
                    minutes=rng.int(0, 60),
                )

                if ts < plan.start_date or ts >= end_excl:
                    continue

                month_candidates.append((ts, eligible, amount))

        for ts, eligible, amount in sorted(month_candidates, key=lambda x: x[0]):
            seed_idx = advance_book_through(
                book,
                seeded,
                seed_idx,
                ts,
                inclusive=True,
            )
            src = _choose_source_account(rng, eligible, amount, book)
            if src is None:
                continue

            dst = _choose_destination_account(rng, src, eligible, book)
            if dst is None:
                continue

            if book is not None:
                decision = book.try_transfer_with_reason(
                    src,
                    dst,
                    amount,
                    channel=SELF_TRANSFER,
                    timestamp=ts,
                )
                if not decision.accepted:
                    continue

            txns.append(
                txf.make(
                    TransactionDraft(
                        source=src,
                        destination=dst,
                        amount=amount,
                        timestamp=ts,
                        channel=SELF_TRANSFER,
                    )
                )
            )

    return txns
