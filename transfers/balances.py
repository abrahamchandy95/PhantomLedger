from dataclasses import dataclass

import numpy as np
import numpy.typing as npt

from common.ids import is_external
from common.math import (
    Bool,
    F64,
    I64,
    lognormal_by_median,
)
from common.random import Rng
from common.validate import between, ge
from entities.personas import get_persona


REJECT_INVALID_AMOUNT = "invalid_amount"
REJECT_EXTERNAL_TO_EXTERNAL = "external_to_external"
REJECT_INSUFFICIENT_FUNDS = "insufficient_funds"


@dataclass(frozen=True, slots=True)
class TransferDecision:
    accepted: bool
    reason: str | None = None


@dataclass(frozen=True, slots=True)
class Rules:
    enable_constraints: bool = True

    overdraft_fraction: float = 0.05
    overdraft_limit_median: float = 300.0
    overdraft_limit_sigma: float = 0.6

    initial_balance_sigma: float = 1.0

    def __post_init__(self) -> None:
        between("overdraft_fraction", self.overdraft_fraction, 0.0, 1.0)
        ge("overdraft_limit_median", self.overdraft_limit_median, 0.0)
        ge("overdraft_limit_sigma", self.overdraft_limit_sigma, 0.0)
        ge("initial_balance_sigma", self.initial_balance_sigma, 0.0)

    def initial_balance_median(self, persona: str) -> float:
        return float(get_persona(persona).initial_balance)


DEFAULT_RULES = Rules()


@dataclass(frozen=True, slots=True)
class SetupParams:
    accounts: list[str]
    account_indices: dict[str, int]
    hub_indices: set[int]
    persona_mapping: npt.NDArray[np.integer]
    persona_names: list[str]


def _sample_balances(
    rules: Rules,
    rng: Rng,
    *,
    num_accounts: int,
    persona_mapping: npt.NDArray[np.integer],
    persona_names: list[str],
) -> F64:
    balances: F64 = np.zeros(num_accounts, dtype=np.float64)
    persona_arr: I64 = np.asarray(persona_mapping, dtype=np.int64)

    sigma = float(rules.initial_balance_sigma)

    for persona_id, persona_name in enumerate(persona_names):
        mask: Bool = np.asarray(persona_arr == persona_id, dtype=np.bool_)
        count = int(np.count_nonzero(mask))
        if count <= 0:
            continue

        balances[mask] = lognormal_by_median(
            rng.gen,
            median=rules.initial_balance_median(persona_name),
            sigma=sigma,
            size=count,
        )

    return balances


def _sample_overdrafts(
    rules: Rules,
    rng: Rng,
    *,
    num_accounts: int,
    hub_indices: set[int],
) -> F64:
    overdrafts: F64 = np.zeros(num_accounts, dtype=np.float64)
    fraction = float(rules.overdraft_fraction)

    if fraction <= 0.0:
        return overdrafts

    eligible_mask = np.ones(num_accounts, dtype=np.bool_)
    if hub_indices:
        eligible_mask[list(hub_indices)] = False

    eligible_idx = np.flatnonzero(eligible_mask)
    eligible_count = eligible_idx.size

    k = int(fraction * eligible_count)
    if k <= 0:
        return overdrafts

    chosen: I64 = np.asarray(
        rng.gen.choice(eligible_idx, size=k, replace=False),
        dtype=np.int64,
    )

    overdrafts[chosen] = lognormal_by_median(
        rng.gen,
        median=float(rules.overdraft_limit_median),
        sigma=float(rules.overdraft_limit_sigma),
        size=int(chosen.size),
    )

    return overdrafts


@dataclass(slots=True)
class Ledger:
    """Manages internal state for account balances and transfer mechanics."""

    balances: F64
    overdrafts: F64
    account_indices: dict[str, int]
    hub_indices: set[int]
    external_indices: set[int]

    def copy(self) -> "Ledger":
        return Ledger(
            balances=np.array(self.balances, copy=True, dtype=np.float64),
            overdrafts=np.array(self.overdrafts, copy=True, dtype=np.float64),
            account_indices=dict(self.account_indices),
            hub_indices=set(self.hub_indices),
            external_indices=set(self.external_indices),
        )

    def _index_of(self, account: str) -> int | None:
        return self.account_indices.get(account)

    def set_credit_limit(self, account: str, limit_value: float) -> None:
        idx = self._index_of(account)
        if idx is None:
            return
        self.balances[idx] = 0.0
        self.overdrafts[idx] = float(limit_value)

    def try_transfer(self, src: str, dst: str, amount: float) -> bool:
        return self.try_transfer_with_reason(src, dst, amount).accepted

    def try_transfer_with_reason(
        self,
        src: str,
        dst: str,
        amount: float,
    ) -> TransferDecision:
        """Execute a transfer if allowed, returning the drop reason on failure."""
        amt = float(amount)
        if amt <= 0.0 or not np.isfinite(amt):
            return TransferDecision(False, REJECT_INVALID_AMOUNT)

        src_idx = self._index_of(src)
        dst_idx = self._index_of(dst)

        src_ext = src_idx is not None and src_idx in self.external_indices
        dst_ext = dst_idx is not None and dst_idx in self.external_indices

        # Unknown accounts are treated as external counterparties.
        if src_idx is None:
            src_ext = True
        if dst_idx is None:
            dst_ext = True

        if src_ext and dst_ext:
            return TransferDecision(False, REJECT_EXTERNAL_TO_EXTERNAL)

        balances = self.balances
        overdrafts = self.overdrafts
        hubs = self.hub_indices

        # Case 1: Inbound from external into a known internal account.
        if src_ext:
            assert dst_idx is not None
            balances[dst_idx] += amt
            return TransferDecision(True)

        # src is internal from here onward.
        assert src_idx is not None
        is_hub = src_idx in hubs

        if not is_hub and (balances[src_idx] + overdrafts[src_idx] < amt):
            return TransferDecision(False, REJECT_INSUFFICIENT_FUNDS)

        # Case 2: Outbound to external.
        if dst_ext:
            if not is_hub:
                balances[src_idx] -= amt
            return TransferDecision(True)

        # Case 3: Internal P2P.
        assert dst_idx is not None

        if not is_hub:
            balances[src_idx] -= amt

        balances[dst_idx] += amt
        return TransferDecision(True)


def initialize(
    rules: Rules,
    rng: Rng,
    params: SetupParams,
) -> Ledger:
    """Entry point to bootstrap a new Ledger with randomized starting state."""
    num_accounts = len(params.accounts)

    balances = _sample_balances(
        rules,
        rng,
        num_accounts=num_accounts,
        persona_mapping=params.persona_mapping,
        persona_names=params.persona_names,
    )

    if params.hub_indices:
        balances[list(params.hub_indices)] = 1e18

    overdrafts = _sample_overdrafts(
        rules,
        rng,
        num_accounts=num_accounts,
        hub_indices=params.hub_indices,
    )

    # Pre-compute external account indices for O(1) lookup in hot path
    external_indices = {
        idx for acct, idx in params.account_indices.items() if is_external(acct)
    }

    return Ledger(
        balances=balances,
        overdrafts=overdrafts,
        account_indices=params.account_indices,
        hub_indices=params.hub_indices,
        external_indices=external_indices,
    )
