"""
Intra-person account transfers.

People with multiple accounts at the same bank regularly move money
between them: checking-to-savings, rebalancing, paying a credit card
from a secondary account, etc.

These appear as high-frequency, variable-amount transfers between
accounts with the same owner — a pattern that looks superficially
like fraud layering if not modeled as legitimate activity.

Only applies to people with 2+ accounts (config max_per_person >= 2).
"""

from dataclasses import dataclass
from datetime import timedelta

from common.channels import SELF_TRANSFER
from common.random import Rng
from common.transactions import Transaction
from common.validate import between, ge, gt
from common.math import lognormal_by_median
from transfers.factory import TransactionDraft, TransactionFactory

from .plans import LegitBuildPlan


@dataclass(frozen=True, slots=True)
class SelfTransferConfig:
    """Controls intra-person transfer generation."""

    # Monthly probability of making self-transfers (per person with 2+ accounts)
    active_p: float = 0.70

    transfers_per_month_min: int = 1
    transfers_per_month_max: int = 4

    amount_median: float = 250.0
    amount_sigma: float = 0.8

    # Probability of using a round amount ($50, $100, $200, etc)
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


def generate(
    rng: Rng,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
    accounts_by_person: dict[str, list[str]],
    cfg: SelfTransferConfig = DEFAULT_SELF_TRANSFER_CONFIG,
) -> list[Transaction]:
    """Generates transfers between accounts owned by the same person."""
    if not plan.paydays:
        return []

    end_excl = plan.start_date + timedelta(days=plan.days)
    hub_set = plan.counterparties.hub_set
    txns: list[Transaction] = []

    for person_id in plan.persons:
        accts = accounts_by_person.get(person_id, [])

        # Filter to non-hub internal accounts
        eligible = [a for a in accts if a not in hub_set]
        if len(eligible) < 2:
            continue

        if not rng.coin(cfg.active_p):
            continue

        for month_start in plan.paydays:
            n_transfers = rng.int(
                cfg.transfers_per_month_min,
                cfg.transfers_per_month_max + 1,
            )

            for _ in range(n_transfers):
                # Pick source and destination from the person's own accounts
                src = rng.choice(eligible)
                dst = rng.choice([a for a in eligible if a != src])

                if rng.coin(cfg.round_amount_p):
                    amount = rng.choice(_ROUND_AMOUNTS)
                else:
                    amount = float(
                        lognormal_by_median(
                            rng.gen,
                            median=cfg.amount_median,
                            sigma=cfg.amount_sigma,
                        )
                    )
                    amount = round(max(10.0, amount), 2)

                day_offset = rng.int(0, 28)
                ts = month_start + timedelta(
                    days=day_offset,
                    hours=rng.int(8, 22),
                    minutes=rng.int(0, 60),
                )

                if ts < plan.start_date or ts >= end_excl:
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
