"""
Direct deposit splitting across accounts.

~30% of workers with multiple accounts split their direct deposit
(APA 2024). Typically a fixed amount or percentage goes to a savings
account each pay period. This creates regular internal transfers
that look like self-transfers but are triggered by payroll.
"""

from datetime import timedelta

from common.channels import SELF_TRANSFER
from common.random import Rng
from common.transactions import Transaction
from transfers.factory import TransactionDraft, TransactionFactory

from transfers.legit.blueprints import LegitBuildPlan


def split_deposits(
    rng: Rng,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
    accounts_by_person: dict[str, list[str]],
    existing_txns: list[Transaction],
) -> list[Transaction]:
    """
    For people with 2+ accounts, split a portion of each salary
    deposit into a secondary account.
    """
    # Find people with multiple non-hub accounts
    multi_acct: dict[str, list[str]] = {}
    hub_set = plan.counterparties.hub_set

    for person_id in plan.persons:
        accts = accounts_by_person.get(person_id, [])
        eligible = [a for a in accts if a not in hub_set]
        if len(eligible) >= 2:
            multi_acct[person_id] = eligible

    if not multi_acct:
        return []

    # Pre-determine which people split deposits (~30%)
    splitters: dict[str, tuple[str, str, float]] = {}
    for person_id, accts in multi_acct.items():
        if not rng.coin(0.30):
            continue

        primary = accts[0]
        secondary = accts[1]

        # Fixed percentage: 10-35% goes to secondary
        split_frac = 0.10 + 0.25 * rng.float()
        splitters[person_id] = (primary, secondary, split_frac)

    if not splitters:
        return []

    # Scan existing salary txns and generate splits
    primary_to_person: dict[str, str] = {}
    for person_id, (primary, _, _) in splitters.items():
        primary_to_person[primary] = person_id

    txns: list[Transaction] = []

    for txn in existing_txns:
        if txn.channel != "salary":
            continue

        person_id = primary_to_person.get(txn.target)
        if person_id is None:
            continue

        _, secondary, frac = splitters[person_id]
        split_amt = round(float(txn.amount) * frac, 2)

        if split_amt < 10.0:
            continue

        # Transfer happens shortly after salary arrives
        ts = txn.timestamp + timedelta(
            minutes=rng.int(5, 30),
        )

        txns.append(
            txf.make(
                TransactionDraft(
                    source=txn.target,
                    destination=secondary,
                    amount=split_amt,
                    timestamp=ts,
                    channel=SELF_TRANSFER,
                )
            )
        )

    return txns
