"""
Shared constants and helpers for the AML export module.
"""

from __future__ import annotations

from .identity_aml import stable_u64

OUR_BANK_ID: str = "BNK_0000"


def counterparty_bank_id(cp_id: str) -> str:
    """Deterministic bank assignment for a counterparty."""
    x = stable_u64(cp_id, "bank")
    return f"BNK_{(x % 20) + 1:04d}"


def all_bank_ids(external_ids: set[str]) -> set[str]:
    """Collect our bank plus all synthetic bank IDs referenced by counterparties."""
    seen: set[str] = {OUR_BANK_ID}
    for cp_id in external_ids:
        seen.add(counterparty_bank_id(cp_id))
    return seen
