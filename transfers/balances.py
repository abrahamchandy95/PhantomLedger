from dataclasses import dataclass
from typing import cast

import numpy as np
import numpy.typing as npt

from common.ids import is_external_account
from common.math import (
    ArrBool,
    ArrF64,
    ArrI64,
    NumScalar,
    as_float,
    lognormal_by_median,
)
from common.random import Rng
from common.validate import (
    require_float_between,
    require_float_ge,
)


@dataclass(frozen=True, slots=True)
class BalancePolicy:
    enable_balance_constraints: bool = True

    overdraft_frac: float = 0.05
    overdraft_limit_median: float = 300.0
    overdraft_limit_sigma: float = 0.6

    init_bal_student: float = 200.0
    init_bal_salaried: float = 1200.0
    init_bal_retired: float = 1500.0
    init_bal_freelancer: float = 900.0
    init_bal_smallbiz: float = 8000.0
    init_bal_hnw: float = 25000.0
    init_bal_sigma: float = 1.0

    def validate(self) -> None:
        require_float_between("overdraft_frac", self.overdraft_frac, 0.0, 1.0)
        require_float_ge("overdraft_limit_median", self.overdraft_limit_median, 0.0)
        require_float_ge("overdraft_limit_sigma", self.overdraft_limit_sigma, 0.0)
        require_float_ge("init_bal_sigma", self.init_bal_sigma, 0.0)

    def initial_balance_median(self, persona: str) -> float:
        match persona:
            case "student":
                return float(self.init_bal_student)
            case "retired":
                return float(self.init_bal_retired)
            case "freelancer":
                return float(self.init_bal_freelancer)
            case "smallbiz":
                return float(self.init_bal_smallbiz)
            case "hnw":
                return float(self.init_bal_hnw)
            case _:
                return float(self.init_bal_salaried)


DEFAULT_BALANCE_POLICY = BalancePolicy()


@dataclass(frozen=True, slots=True)
class BalanceInitRequest:
    accounts: list[str]
    acct_index: dict[str, int]
    hub_set_idx: set[int]
    persona_for_acct: npt.NDArray[np.integer]
    persona_names: list[str]


def _scalar_at(arr: ArrF64, idx: int) -> float:
    return as_float(cast(NumScalar, arr[idx]))


def _eligible_non_hub_mask(n: int, hub_set_idx: set[int]) -> ArrBool:
    mask = np.ones(n, dtype=np.bool_)
    for idx in hub_set_idx:
        mask[idx] = False
    return mask


def _sample_initial_balances(
    policy: BalancePolicy,
    rng: Rng,
    *,
    n_accounts: int,
    persona_for_acct: npt.NDArray[np.integer],
    persona_names: list[str],
) -> ArrF64:
    balances: ArrF64 = np.zeros(n_accounts, dtype=np.float64)
    persona_arr: ArrI64 = np.asarray(persona_for_acct, dtype=np.int64)

    sigma = float(policy.init_bal_sigma)

    for persona_id, persona_name in enumerate(persona_names):
        mask: ArrBool = np.asarray(persona_arr == int(persona_id), dtype=np.bool_)
        count = int(np.count_nonzero(mask))
        if count <= 0:
            continue

        balances[mask] = lognormal_by_median(
            rng.gen,
            median=policy.initial_balance_median(persona_name),
            sigma=sigma,
            size=count,
        )

    return balances


def _sample_overdraft_limits(
    policy: BalancePolicy,
    rng: Rng,
    *,
    n_accounts: int,
    hub_set_idx: set[int],
) -> ArrF64:
    overdraft_limit: ArrF64 = np.zeros(n_accounts, dtype=np.float64)
    overdraft_frac = float(policy.overdraft_frac)

    if overdraft_frac <= 0.0:
        return overdraft_limit

    eligible_mask = _eligible_non_hub_mask(n_accounts, hub_set_idx)
    eligible_idx: ArrI64 = np.asarray(np.nonzero(eligible_mask)[0], dtype=np.int64)
    eligible_count = int(eligible_idx.size)

    k = int(overdraft_frac * eligible_count)
    if k <= 0:
        return overdraft_limit

    chosen: ArrI64 = np.asarray(
        cast(object, rng.gen.choice(eligible_idx, size=k, replace=False)),
        dtype=np.int64,
    )

    overdraft_limit[chosen] = lognormal_by_median(
        rng.gen,
        median=float(policy.overdraft_limit_median),
        sigma=float(policy.overdraft_limit_sigma),
        size=int(chosen.size),
    )

    return overdraft_limit


@dataclass(slots=True)
class BalanceBook:
    balances: ArrF64
    overdraft_limit: ArrF64
    acct_index: dict[str, int]
    hub_set_idx: set[int]

    def _index_of(self, acct: str) -> int | None:
        return self.acct_index.get(acct)

    def _balance_at(self, idx: int) -> float:
        return _scalar_at(self.balances, idx)

    def _overdraft_at(self, idx: int) -> float:
        return _scalar_at(self.overdraft_limit, idx)

    def _credit_idx(self, idx: int, amount: float) -> None:
        self.balances[idx] = self._balance_at(idx) + amount

    def _can_debit_idx(self, idx: int, amount: float) -> bool:
        if idx in self.hub_set_idx:
            return True
        return self._balance_at(idx) + self._overdraft_at(idx) >= amount

    def _debit_idx(self, idx: int, amount: float) -> bool:
        if idx in self.hub_set_idx:
            return True
        if not self._can_debit_idx(idx, amount):
            return False

        self.balances[idx] = self._balance_at(idx) - amount
        return True

    def set_credit_limit(self, acct: str, limit_value: float) -> None:
        idx = self._index_of(acct)
        if idx is None:
            return
        self.balances[idx] = 0.0
        self.overdraft_limit[idx] = float(limit_value)

    def try_transfer(self, src: str, dst: str, amount: float) -> bool:
        amt = float(amount)
        if amt <= 0.0 or not np.isfinite(amt):
            return False

        src_ext = is_external_account(src)
        dst_ext = is_external_account(dst)

        if src_ext and dst_ext:
            return False

        if src_ext and not dst_ext:
            dst_idx = self._index_of(dst)
            if dst_idx is None:
                return False

            self._credit_idx(dst_idx, amt)
            return True

        if not src_ext and dst_ext:
            src_idx = self._index_of(src)
            if src_idx is None:
                return False

            return self._debit_idx(src_idx, amt)

        src_idx = self._index_of(src)
        dst_idx = self._index_of(dst)
        if src_idx is None or dst_idx is None:
            return False

        if not self._debit_idx(src_idx, amt):
            return False

        self._credit_idx(dst_idx, amt)
        return True


def init_balances(
    policy: BalancePolicy,
    rng: Rng,
    request: BalanceInitRequest,
) -> BalanceBook:
    policy.validate()

    n_accounts = len(request.accounts)

    balances = _sample_initial_balances(
        policy,
        rng,
        n_accounts=n_accounts,
        persona_for_acct=request.persona_for_acct,
        persona_names=request.persona_names,
    )

    for idx in request.hub_set_idx:
        balances[idx] = 1e18

    overdraft_limit = _sample_overdraft_limits(
        policy,
        rng,
        n_accounts=n_accounts,
        hub_set_idx=request.hub_set_idx,
    )

    return BalanceBook(
        balances=balances,
        overdraft_limit=overdraft_limit,
        acct_index=request.acct_index,
        hub_set_idx=request.hub_set_idx,
    )
