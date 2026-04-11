"""
Sibling-to-sibling transfers.

Siblings are the most common borrowers in family lending (31% of
all family loans, FinanceBuzz 2024). 76% of people have borrowed
from a sibling at some point (JG Wentworth 2025). Median amount
owed between siblings: ~$400 (LendingTree 2021).

These are irregular transfers — splitting bills, lending for rent,
repaying small debts — not monthly like salary or allowance.

Siblings are identified as non-spouse, non-student adults sharing
a household in the family graph.
"""

from datetime import timedelta
from typing import cast

import numpy as np

from common.channels import SIBLING_TRANSFER
from common.family_accounts import resolve_family_acct
from common.math import as_int, lognormal_by_median
from common.transactions import Transaction
from transfers.factory import TransactionDraft

from .engine import GenerateRequest, Schedule


def _find_sibling_pairs(
    request: GenerateRequest,
) -> list[tuple[str, str]]:
    """Finds adult non-spouse pairs within each household."""
    spouses = request.family.spouses
    pairs: list[tuple[str, str]] = []
    seen: set[tuple[str, str]] = set()

    for hh in request.family.households:
        adults = [p for p in hh if request.personas.get(p, "salaried") != "student"]
        if len(adults) < 2:
            continue

        for i, a in enumerate(adults):
            for b in adults[i + 1 :]:
                # Skip if they're spouses
                if spouses.get(a) == b:
                    continue
                pair = (min(a, b), max(a, b))
                if pair not in seen:
                    seen.add(pair)
                    pairs.append(pair)

    return pairs


def generate(
    request: GenerateRequest,
    schedule: Schedule,
    gen: np.random.Generator,
) -> list[Transaction]:
    """Generates irregular transfers between adult siblings."""
    params = request.params
    if not params.sibling_transfer_enabled:
        return []

    pairs = _find_sibling_pairs(request)
    if not pairs:
        return []

    txns: list[Transaction] = []
    external_p = float(params.external_family_p)

    for person_a, person_b in pairs:
        # Only a fraction of sibling pairs actively exchange money
        if float(gen.random()) >= float(params.sibling_active_p):
            continue

        acct_a = resolve_family_acct(person_a, request.primary_accounts, external_p)
        acct_b = resolve_family_acct(person_b, request.primary_accounts, external_p)

        if not acct_a or not acct_b or acct_a == acct_b:
            continue
        if acct_a.startswith("XF") and acct_b.startswith("XF"):
            continue

        for month_start in schedule.month_starts:
            if float(gen.random()) >= float(params.sibling_monthly_p):
                continue

            # Direction is roughly random between siblings
            if float(gen.random()) < 0.5:
                src, dst = acct_a, acct_b
            else:
                src, dst = acct_b, acct_a

            amt = float(
                lognormal_by_median(
                    gen,
                    median=float(params.sibling_median),
                    sigma=float(params.sibling_sigma),
                )
            )
            amt = round(max(5.0, amt), 2)

            offset_days = as_int(cast(int | np.integer, gen.integers(0, 28)))
            offset_hours = as_int(cast(int | np.integer, gen.integers(8, 22)))
            offset_mins = as_int(cast(int | np.integer, gen.integers(0, 60)))

            ts = month_start + timedelta(
                days=offset_days,
                hours=offset_hours,
                minutes=offset_mins,
            )
            if ts >= schedule.end_excl:
                break

            txns.append(
                request.txf.make(
                    TransactionDraft(
                        source=src,
                        destination=dst,
                        amount=amt,
                        timestamp=ts,
                        channel=SIBLING_TRANSFER,
                    )
                )
            )

    return txns
