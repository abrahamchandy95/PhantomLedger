"""
ATM cash withdrawals.

ATM withdrawals are high-frequency transactions in real bank data
with characteristic round amounts. They appear as outflows to the
bank's own ATM network clearing account.

This version adds a lightweight affordability screen using a seeded
ledger view plus known earlier candidate transactions. It is still
an upstream proxy, not a substitute for final replay.
"""

from collections.abc import Sequence
from dataclasses import dataclass
from datetime import datetime, timedelta

from common.channels import ATM
from common.ids import is_external
from common.random import Rng
from common.transactions import Transaction
from common.validate import between, ge
from transfers.balances import Ledger
from transfers.factory import TransactionDraft, TransactionFactory
from transfers.screening import advance_book_through

from transfers.legit.blueprints import LegitBuildPlan

_ATM_AMOUNTS: tuple[float, ...] = (
    20.0,
    40.0,
    40.0,
    40.0,
    60.0,
    60.0,
    60.0,
    80.0,
    80.0,
    100.0,
    100.0,
    100.0,
    120.0,
    140.0,
    160.0,
    200.0,
    200.0,
    300.0,
)


@dataclass(frozen=True, slots=True)
class AtmConfig:
    user_p: float = 0.88
    withdrawals_per_month_min: int = 1
    withdrawals_per_month_max: int = 6

    def __post_init__(self) -> None:
        between("user_p", self.user_p, 0.0, 1.0)
        ge("withdrawals_per_month_min", self.withdrawals_per_month_min, 1)
        ge(
            "withdrawals_per_month_max",
            self.withdrawals_per_month_max,
            self.withdrawals_per_month_min,
        )


DEFAULT_ATM_CONFIG = AtmConfig()


def _can_afford_atm(book: Ledger | None, account: str, amount: float) -> bool:
    if book is None:
        return True

    available = book.available_to_spend(account)
    # Keep a small post-withdrawal cushion; ATM is highly discretionary.
    reserve = max(40.0, min(120.0, float(amount) * 0.50))
    return available >= float(amount) + reserve


def generate(
    rng: Rng,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
    cfg: AtmConfig = DEFAULT_ATM_CONFIG,
    *,
    book: Ledger | None = None,
    base_txns: Sequence[Transaction] | None = None,
    base_txns_sorted: bool = False,
) -> list[Transaction]:
    if not plan.paydays:
        return []

    if not plan.counterparties.hub_accounts:
        return []
    atm_network_acct = plan.counterparties.hub_accounts[0]

    end_excl = plan.start_date + timedelta(days=plan.days)
    txns: list[Transaction] = []

    if base_txns is None:
        seeded: Sequence[Transaction] = ()
    elif base_txns_sorted:
        seeded = base_txns
    else:
        seeded = sorted(base_txns, key=lambda t: t.timestamp)
    seed_idx = 0

    users: list[tuple[str, str]] = []
    for person_id in plan.persons:
        deposit_acct = plan.primary_acct_for_person.get(person_id)
        if not deposit_acct:
            continue
        if deposit_acct in plan.counterparties.hub_set:
            continue
        if is_external(deposit_acct):
            continue
        if not rng.coin(cfg.user_p):
            continue
        users.append((person_id, deposit_acct))

    for month_start in plan.paydays:
        month_candidates: list[tuple[datetime, str, float]] = []

        for _, deposit_acct in users:
            n_withdrawals = rng.int(
                cfg.withdrawals_per_month_min,
                cfg.withdrawals_per_month_max + 1,
            )

            for _ in range(n_withdrawals):
                amount = float(rng.choice(_ATM_AMOUNTS))

                # Suppress some very late-cycle ATM attempts upstream.
                if rng.float() < 0.75:
                    day_offset = rng.int(0, 18)
                else:
                    day_offset = rng.int(18, 28)

                ts = month_start + timedelta(
                    days=day_offset,
                    hours=rng.int(7, 23),
                    minutes=rng.int(0, 60),
                )

                if ts < plan.start_date or ts >= end_excl:
                    continue

                month_candidates.append((ts, deposit_acct, amount))

        for ts, deposit_acct, amount in sorted(month_candidates, key=lambda x: x[0]):
            seed_idx = advance_book_through(
                book,
                seeded,
                seed_idx,
                ts,
                inclusive=True,
            )
            if not _can_afford_atm(book, deposit_acct, amount):
                continue

            if book is not None:
                decision = book.try_transfer_with_reason(
                    deposit_acct,
                    atm_network_acct,
                    amount,
                    channel=ATM,
                    timestamp=ts,
                )
                if not decision.accepted:
                    continue

            txns.append(
                txf.make(
                    TransactionDraft(
                        source=deposit_acct,
                        destination=atm_network_acct,
                        amount=amount,
                        timestamp=ts,
                        channel=ATM,
                    )
                )
            )

    return txns
