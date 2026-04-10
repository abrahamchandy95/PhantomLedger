"""
ATM cash withdrawals.

ATM withdrawals are high-frequency transactions in real bank data
with characteristic round amounts. They appear as outflows to the
bank's own ATM network clearing account.

Statistics (Federal Reserve Payments Study, ATM Marketplace):
- US consumers average 3.8 ATM withdrawals/month
- Average withdrawal amount: $80-$120
- Common denominations: $20, $40, $60, $80, $100, $200, $300
- 88% of US households use ATMs
- Usage skews younger: ages 18-34 average 5.8/month
"""

from dataclasses import dataclass
from datetime import timedelta

from common.channels import ATM
from common.ids import is_external
from common.random import Rng
from common.transactions import Transaction
from common.validate import between, ge
from transfers.factory import TransactionDraft, TransactionFactory

from .plans import LegitBuildPlan

# Common ATM withdrawal amounts, weighted by frequency.
# $40 and $60 are the most common; $200+ is rarer.
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

# The ATM network account is the first hub account (the bank itself).
# All ATM withdrawals debit the customer and credit this account.


@dataclass(frozen=True, slots=True)
class AtmConfig:
    """Controls ATM withdrawal generation."""

    # Probability that a person uses ATMs at all (88% of US households)
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


def generate(
    rng: Rng,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
    cfg: AtmConfig = DEFAULT_ATM_CONFIG,
) -> list[Transaction]:
    """Generates ATM cash withdrawal transactions."""
    if not plan.paydays:
        return []

    # ATM network account: first hub account represents the bank itself
    if not plan.counterparties.hub_accounts:
        return []
    atm_network_acct = plan.counterparties.hub_accounts[0]

    end_excl = plan.start_date + timedelta(days=plan.days)
    txns: list[Transaction] = []

    for person_id in plan.persons:
        deposit_acct = plan.primary_acct_for_person.get(person_id)
        if not deposit_acct:
            continue
        if deposit_acct in plan.counterparties.hub_set:
            continue
        if is_external(deposit_acct):
            continue

        # Does this person use ATMs?
        if not rng.coin(cfg.user_p):
            continue

        for month_start in plan.paydays:
            n_withdrawals = rng.int(
                cfg.withdrawals_per_month_min,
                cfg.withdrawals_per_month_max + 1,
            )

            for _ in range(n_withdrawals):
                amount = rng.choice(_ATM_AMOUNTS)

                day_offset = rng.int(0, 28)
                ts = month_start + timedelta(
                    days=day_offset,
                    hours=rng.int(7, 23),
                    minutes=rng.int(0, 60),
                )

                if ts < plan.start_date or ts >= end_excl:
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
