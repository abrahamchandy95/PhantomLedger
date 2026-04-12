from collections.abc import Callable
from dataclasses import dataclass
from datetime import datetime
from typing import cast

import numpy as np
import numpy.typing as npt

from common.channels import SELF_TRANSFER
from common.ids import is_external
from common.math import Bool, F64, I64, lognormal_by_median
from common.persona_names import (
    FREELANCER,
    HNW,
    RETIRED,
    SALARIED,
    SMALLBIZ,
    STUDENT,
)
from common.random import Rng
from common.validate import ge
from entities.personas import get_persona

REJECT_INVALID_AMOUNT = "invalid_amount"
REJECT_EXTERNAL_TO_EXTERNAL = "external_to_external"
REJECT_INSUFFICIENT_FUNDS = "insufficient_funds"

type PersonaFloatLookup = Callable[[str], float]


@dataclass(frozen=True, slots=True)
class TransferDecision:
    accepted: bool
    reason: str | None = None


@dataclass(frozen=True, slots=True)
class Rules:
    enable_constraints: bool = True

    # Core overdraft line.
    overdraft_limit_sigma: float = 0.60

    # Linked-account protection / sweep buffer.
    linked_buffer_sigma: float = 0.90

    # Courtesy cushion used by some institutions for borderline shortfalls.
    courtesy_buffer_sigma: float = 0.45

    initial_balance_sigma: float = 1.00

    def __post_init__(self) -> None:
        ge("overdraft_limit_sigma", self.overdraft_limit_sigma, 0.0)
        ge("linked_buffer_sigma", self.linked_buffer_sigma, 0.0)
        ge("courtesy_buffer_sigma", self.courtesy_buffer_sigma, 0.0)
        ge("initial_balance_sigma", self.initial_balance_sigma, 0.0)

    def initial_balance_median(self, persona: str) -> float:
        return float(get_persona(persona).initial_balance)

    def overdraft_fraction_for_persona(self, persona: str) -> float:
        table = {
            STUDENT: 0.20,
            RETIRED: 0.28,
            SALARIED: 0.42,
            FREELANCER: 0.34,
            SMALLBIZ: 0.46,
            HNW: 0.55,
        }
        return float(table.get(persona, table[SALARIED]))

    def overdraft_limit_median_for_persona(self, persona: str) -> float:
        table = {
            STUDENT: 350.0,
            RETIRED: 600.0,
            SALARIED: 900.0,
            FREELANCER: 800.0,
            SMALLBIZ: 2200.0,
            HNW: 5000.0,
        }
        return float(table.get(persona, table[SALARIED]))

    def linked_buffer_fraction_for_persona(self, persona: str) -> float:
        table = {
            STUDENT: 0.16,
            RETIRED: 0.30,
            SALARIED: 0.32,
            FREELANCER: 0.26,
            SMALLBIZ: 0.34,
            HNW: 0.60,
        }
        return float(table.get(persona, table[SALARIED]))

    def linked_buffer_median_for_persona(self, persona: str) -> float:
        table = {
            STUDENT: 225.0,
            RETIRED: 500.0,
            SALARIED: 700.0,
            FREELANCER: 600.0,
            SMALLBIZ: 2200.0,
            HNW: 10000.0,
        }
        return float(table.get(persona, table[SALARIED]))

    def courtesy_buffer_fraction_for_persona(self, persona: str) -> float:
        table = {
            STUDENT: 0.12,
            RETIRED: 0.16,
            SALARIED: 0.18,
            FREELANCER: 0.16,
            SMALLBIZ: 0.20,
            HNW: 0.22,
        }
        return float(table.get(persona, table[SALARIED]))

    def courtesy_buffer_median_for_persona(self, persona: str) -> float:
        table = {
            STUDENT: 65.0,
            RETIRED: 100.0,
            SALARIED: 140.0,
            FREELANCER: 120.0,
            SMALLBIZ: 180.0,
            HNW: 250.0,
        }
        return float(table.get(persona, table[SALARIED]))


DEFAULT_RULES = Rules()


@dataclass(frozen=True, slots=True)
class SetupParams:
    accounts: list[str]
    account_indices: dict[str, int]
    hub_indices: set[int]
    persona_mapping: npt.NDArray[np.int64]
    persona_names: list[str]


def _sample_balances(
    rules: Rules,
    rng: Rng,
    *,
    num_accounts: int,
    persona_mapping: npt.NDArray[np.int64],
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

        sampled = lognormal_by_median(
            rng.gen,
            median=rules.initial_balance_median(persona_name),
            sigma=sigma,
            size=count,
        )
        balances[mask] = np.asarray(sampled, dtype=np.float64)

    return balances


def _sample_selected_indices(
    rng: Rng,
    eligible_idx: npt.NDArray[np.int64],
    k: int,
) -> npt.NDArray[np.int64]:
    if k <= 0 or eligible_idx.size == 0:
        return np.empty(0, dtype=np.int64)

    chosen = np.asarray(
        rng.gen.choice(eligible_idx, size=k, replace=False),
        dtype=np.int64,
    )
    return chosen


def _sample_limits_by_persona(
    rng: Rng,
    *,
    num_accounts: int,
    hub_indices: set[int],
    persona_mapping: npt.NDArray[np.int64],
    persona_names: list[str],
    fraction_for_persona: PersonaFloatLookup,
    median_for_persona: PersonaFloatLookup,
    sigma: float,
) -> F64:
    out: F64 = np.zeros(num_accounts, dtype=np.float64)
    persona_arr: I64 = np.asarray(persona_mapping, dtype=np.int64)

    eligible_mask: Bool = np.ones(num_accounts, dtype=np.bool_)
    if hub_indices:
        eligible_mask[np.asarray(list(hub_indices), dtype=np.int64)] = False

    for persona_id, persona_name in enumerate(persona_names):
        persona_mask: Bool = np.asarray(persona_arr == persona_id, dtype=np.bool_)
        mask: Bool = np.asarray(persona_mask & eligible_mask, dtype=np.bool_)
        eligible_idx = np.asarray(np.flatnonzero(mask), dtype=np.int64)
        eligible_count = int(eligible_idx.size)

        if eligible_count <= 0:
            continue

        fraction = float(fraction_for_persona(persona_name))
        if fraction <= 0.0:
            continue

        k = int(round(fraction * eligible_count))
        if k <= 0:
            continue

        chosen = _sample_selected_indices(rng, eligible_idx, k)
        if chosen.size == 0:
            continue

        sampled = lognormal_by_median(
            rng.gen,
            median=float(median_for_persona(persona_name)),
            sigma=sigma,
            size=int(chosen.size),
        )
        out[chosen] = np.asarray(sampled, dtype=np.float64)

    return out


@dataclass(slots=True)
class Ledger:
    """Manages account balances and available liquidity."""

    balances: F64
    overdrafts: F64
    linked_buffers: F64
    courtesy_buffers: F64
    account_indices: dict[str, int]
    hub_indices: set[int]
    external_indices: set[int]

    def copy(self) -> Ledger:
        return Ledger(
            balances=np.array(self.balances, copy=True, dtype=np.float64),
            overdrafts=np.array(self.overdrafts, copy=True, dtype=np.float64),
            linked_buffers=np.array(self.linked_buffers, copy=True, dtype=np.float64),
            courtesy_buffers=np.array(
                self.courtesy_buffers, copy=True, dtype=np.float64
            ),
            account_indices=dict(self.account_indices),
            hub_indices=set(self.hub_indices),
            external_indices=set(self.external_indices),
        )

    def restore_from(self, other: Ledger) -> None:
        if self.account_indices != other.account_indices:
            raise ValueError(
                "cannot restore from ledger with different account indices"
            )
        if self.hub_indices != other.hub_indices:
            raise ValueError("cannot restore from ledger with different hub indices")
        if self.external_indices != other.external_indices:
            raise ValueError(
                "cannot restore from ledger with different external indices"
            )

        np.copyto(self.balances, other.balances)
        np.copyto(self.overdrafts, other.overdrafts)
        np.copyto(self.linked_buffers, other.linked_buffers)
        np.copyto(self.courtesy_buffers, other.courtesy_buffers)

    def _index_of(self, account: str) -> int | None:
        return self.account_indices.get(account)

    def _available_liquidity(self, idx: int) -> float:
        total = (
            cast(np.float64, self.balances[idx])
            + cast(np.float64, self.overdrafts[idx])
            + cast(np.float64, self.linked_buffers[idx])
            + cast(np.float64, self.courtesy_buffers[idx])
        )
        return float(total)

    def available_to_spend(self, account: str) -> float:
        idx = self._index_of(account)
        if idx is None:
            return 0.0
        if idx in self.hub_indices:
            return float("inf")
        return self._available_liquidity(idx)

    def available_cash(self, account: str) -> float:
        idx = self._index_of(account)
        if idx is None:
            return 0.0
        if idx in self.hub_indices:
            return float("inf")
        return float(cast(np.float64, self.balances[idx]))

    def set_credit_limit(self, account: str, limit_value: float) -> None:
        idx = self._index_of(account)
        if idx is None:
            return

        self.balances[idx] = 0.0
        self.overdrafts[idx] = float(limit_value)
        self.linked_buffers[idx] = 0.0
        self.courtesy_buffers[idx] = 0.0

    def try_transfer(self, src: str, dst: str, amount: float) -> bool:
        return self.try_transfer_with_reason(src, dst, amount).accepted

    def try_transfer_with_reason(
        self,
        src: str,
        dst: str,
        amount: float,
        *,
        channel: str | None = None,
        timestamp: datetime | None = None,
    ) -> TransferDecision:
        del timestamp

        amt = float(amount)
        if amt <= 0.0 or not np.isfinite(amt):
            return TransferDecision(False, REJECT_INVALID_AMOUNT)

        src_idx = self._index_of(src)
        dst_idx = self._index_of(dst)

        src_ext = src_idx is not None and src_idx in self.external_indices
        dst_ext = dst_idx is not None and dst_idx in self.external_indices

        if src_idx is None:
            src_ext = True
        if dst_idx is None:
            dst_ext = True

        if src_ext and dst_ext:
            return TransferDecision(False, REJECT_EXTERNAL_TO_EXTERNAL)

        balances = self.balances
        hubs = self.hub_indices

        # Inbound from external into a known internal account.
        if src_ext:
            if dst_idx is None:
                return TransferDecision(False, REJECT_EXTERNAL_TO_EXTERNAL)
            balances[dst_idx] += amt
            return TransferDecision(True)

        # src is internal from here onward.
        if src_idx is None:
            return TransferDecision(False, REJECT_EXTERNAL_TO_EXTERNAL)

        is_hub = src_idx in hubs
        if not is_hub:
            # Self-transfers must be funded by actual cash, not protection liquidity.
            spendable = (
                float(cast(np.float64, balances[src_idx]))
                if channel == SELF_TRANSFER
                else self._available_liquidity(src_idx)
            )
            if spendable < amt:
                return TransferDecision(False, REJECT_INSUFFICIENT_FUNDS)

        # Outbound to external.
        if dst_ext:
            if not is_hub:
                balances[src_idx] -= amt
            return TransferDecision(True)

        # Internal transfer.
        if dst_idx is None:
            return TransferDecision(False, REJECT_EXTERNAL_TO_EXTERNAL)

        if not is_hub:
            balances[src_idx] -= amt
        balances[dst_idx] += amt
        return TransferDecision(True)


def initialize(
    rules: Rules,
    rng: Rng,
    params: SetupParams,
) -> Ledger:
    """Bootstrap a new Ledger with randomized balances and liquidity buffers."""

    num_accounts = len(params.accounts)

    balances = _sample_balances(
        rules,
        rng,
        num_accounts=num_accounts,
        persona_mapping=params.persona_mapping,
        persona_names=params.persona_names,
    )

    if params.hub_indices:
        hub_idx = np.asarray(list(params.hub_indices), dtype=np.int64)
        balances[hub_idx] = 1e18

    overdrafts = _sample_limits_by_persona(
        rng,
        num_accounts=num_accounts,
        hub_indices=params.hub_indices,
        persona_mapping=params.persona_mapping,
        persona_names=params.persona_names,
        fraction_for_persona=rules.overdraft_fraction_for_persona,
        median_for_persona=rules.overdraft_limit_median_for_persona,
        sigma=float(rules.overdraft_limit_sigma),
    )

    linked_buffers = _sample_limits_by_persona(
        rng,
        num_accounts=num_accounts,
        hub_indices=params.hub_indices,
        persona_mapping=params.persona_mapping,
        persona_names=params.persona_names,
        fraction_for_persona=rules.linked_buffer_fraction_for_persona,
        median_for_persona=rules.linked_buffer_median_for_persona,
        sigma=float(rules.linked_buffer_sigma),
    )

    courtesy_buffers = _sample_limits_by_persona(
        rng,
        num_accounts=num_accounts,
        hub_indices=params.hub_indices,
        persona_mapping=params.persona_mapping,
        persona_names=params.persona_names,
        fraction_for_persona=rules.courtesy_buffer_fraction_for_persona,
        median_for_persona=rules.courtesy_buffer_median_for_persona,
        sigma=float(rules.courtesy_buffer_sigma),
    )

    external_indices = {
        idx for acct, idx in params.account_indices.items() if is_external(acct)
    }

    return Ledger(
        balances=balances,
        overdrafts=overdrafts,
        linked_buffers=linked_buffers,
        courtesy_buffers=courtesy_buffers,
        account_indices=params.account_indices,
        hub_indices=params.hub_indices,
        external_indices=external_indices,
    )
