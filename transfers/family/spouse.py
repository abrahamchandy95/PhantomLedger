"""
Spouse-to-spouse transfers.

In real banking data, intra-couple transfers are one of the highest-volume
legitimate payment channels: shared expense reimbursement, account
rebalancing, "sending money for groceries." Without these, a model sees
high-frequency bidirectional transfers between two accounts only in fraud
rings, when in reality married couples do the same thing.

Statistical basis:
- ~60% of couples have at least partially separate accounts (Census 2023)
- Transfers skew from higher earner to lower earner (Pew 2023: 55% of
  marriages have husband as primary/sole breadwinner)
- We use persona amount_multiplier as a proxy for earning power to
  determine transfer direction without modeling gender explicitly
"""

from datetime import timedelta
from typing import cast

import numpy as np

from common.channels import SPOUSE_TRANSFER
from common.family_accounts import resolve_family_acct
from common.math import as_int, lognormal_by_median
from common.transactions import Transaction
from entities.personas import get_persona
from transfers.factory import TransactionDraft

from .engine import GenerateRequest, Schedule


def _earning_power(persona_name: str) -> float:
    """Use amount_multiplier as a proxy for relative income."""
    return float(get_persona(persona_name).amount_multiplier)


def generate(
    request: GenerateRequest,
    schedule: Schedule,
    gen: np.random.Generator,
) -> list[Transaction]:
    """Generates recurring transfers between spouses with separate accounts."""
    params = request.params
    if not params.spouse_transfer_enabled:
        return []

    txns: list[Transaction] = []
    external_p = float(params.external_family_p)

    # Deduplicate pairs: only process where person_a < person_b
    seen: set[tuple[str, str]] = set()

    for person_a, person_b in request.family.spouses.items():
        pair = (min(person_a, person_b), max(person_a, person_b))
        if pair in seen:
            continue
        seen.add(pair)

        # Check if this couple maintains separate accounts
        if float(gen.random()) >= float(params.spouse_separate_accounts_p):
            continue

        acct_a = resolve_family_acct(person_a, request.primary_accounts, external_p)
        acct_b = resolve_family_acct(person_b, request.primary_accounts, external_p)

        # Skip if either is missing or both are external (invisible to us)
        if not acct_a or not acct_b or acct_a == acct_b:
            continue
        if acct_a.startswith("XF") and acct_b.startswith("XF"):
            continue

        # Determine breadwinner direction using persona earning power
        power_a = _earning_power(request.personas.get(person_a, "salaried"))
        power_b = _earning_power(request.personas.get(person_b, "salaried"))

        if power_a >= power_b:
            higher_acct, lower_acct = acct_a, acct_b
        else:
            higher_acct, lower_acct = acct_b, acct_a

        for month_start in schedule.month_starts:
            n_transfers = as_int(
                cast(
                    int | np.integer,
                    gen.integers(
                        int(params.spouse_txns_per_month_min),
                        int(params.spouse_txns_per_month_max) + 1,
                    ),
                )
            )

            for _ in range(n_transfers):
                # Breadwinner asymmetry: higher earner sends more often
                if float(gen.random()) < float(params.spouse_breadwinner_flow_p):
                    src, dst = higher_acct, lower_acct
                else:
                    src, dst = lower_acct, higher_acct

                offset_days = as_int(cast(int | np.integer, gen.integers(0, 28)))
                offset_hours = as_int(cast(int | np.integer, gen.integers(7, 22)))
                offset_mins = as_int(cast(int | np.integer, gen.integers(0, 60)))

                ts = month_start + timedelta(
                    days=offset_days,
                    hours=offset_hours,
                    minutes=offset_mins,
                )
                if ts >= schedule.end_excl:
                    break

                amt = float(
                    lognormal_by_median(
                        gen,
                        median=float(params.spouse_transfer_median),
                        sigma=float(params.spouse_transfer_sigma),
                    )
                )
                amt = round(max(5.0, amt), 2)

                txns.append(
                    request.txf.make(
                        TransactionDraft(
                            source=src,
                            destination=dst,
                            amount=amt,
                            timestamp=ts,
                            channel=SPOUSE_TRANSFER,
                        )
                    )
                )

    return txns
