from dataclasses import dataclass
from typing import cast

import numpy as np
import numpy.typing as npt

from common.config import BalancesConfig
from common.ids import is_external_account
from common.probability import as_float, lognormal_by_median
from common.rng import Rng


type NumScalar = float | int | np.floating | np.integer
type ArrF64 = npt.NDArray[np.float64]
type ArrI64 = npt.NDArray[np.int64]
type ArrBool = npt.NDArray[np.bool_]


@dataclass(slots=True)
class BalanceBook:
    balances: ArrF64
    overdraft_limit: ArrF64
    acct_index: dict[str, int]
    hub_set_idx: set[int]


def _scalar_at(arr: ArrF64, idx: int) -> float:
    return as_float(cast(NumScalar, arr[idx]))


def init_balances(
    bcfg: BalancesConfig,
    rng: Rng,
    *,
    accounts: list[str],
    acct_index: dict[str, int],
    hub_set_idx: set[int],
    persona_for_acct: npt.NDArray[np.integer],
    persona_names: list[str],
) -> BalanceBook:
    """
    Initialize account balances and overdraft limits.

    Depends only on BalancesConfig: no access to unrelated generation settings.
    """
    n = len(accounts)
    balances: ArrF64 = np.zeros(n, dtype=np.float64)

    sigma = float(bcfg.init_bal_sigma)
    medians: dict[str, float] = {
        "student": float(bcfg.init_bal_student),
        "salaried": float(bcfg.init_bal_salaried),
        "retired": float(bcfg.init_bal_retired),
        "freelancer": float(bcfg.init_bal_freelancer),
        "smallbiz": float(bcfg.init_bal_smallbiz),
        "hnw": float(bcfg.init_bal_hnw),
    }

    persona_arr: ArrI64 = np.asarray(persona_for_acct, dtype=np.int64)

    for pid, pname in enumerate(persona_names):
        median = medians.get(pname, medians["salaried"])

        mask: ArrBool = np.asarray(persona_arr == int(pid), dtype=np.bool_)
        cnt = int(np.count_nonzero(mask))
        if cnt <= 0:
            continue

        balances[mask] = lognormal_by_median(
            rng.gen,
            median=median,
            sigma=sigma,
            size=cnt,
        )

    # hubs get effectively infinite balance
    for i in hub_set_idx:
        balances[i] = 1e18

    overdraft_limit: ArrF64 = np.zeros(n, dtype=np.float64)
    od_frac = float(bcfg.overdraft_frac)

    if od_frac > 0.0:
        elig: ArrBool = np.ones(n, dtype=np.bool_)
        for i in hub_set_idx:
            elig[i] = False

        elig_idx: ArrI64 = np.asarray(np.nonzero(elig)[0], dtype=np.int64)
        elig_count = int(elig_idx.size)
        k = int(od_frac * elig_count)

        if k > 0:
            chosen: ArrI64 = np.asarray(
                cast(object, rng.gen.choice(elig_idx, size=k, replace=False)),
                dtype=np.int64,
            )

            overdraft_limit[chosen] = lognormal_by_median(
                rng.gen,
                median=float(bcfg.overdraft_limit_median),
                sigma=float(bcfg.overdraft_limit_sigma),
                size=int(chosen.size),
            )

    return BalanceBook(
        balances=balances,
        overdraft_limit=overdraft_limit,
        acct_index=acct_index,
        hub_set_idx=hub_set_idx,
    )


def try_transfer(book: BalanceBook, src: str, dst: str, amount: float) -> bool:
    """
    Apply a transfer with support for external counterparties.

    Rules:
      - internal->internal: debit src, credit dst
      - internal->external: debit src only
      - external->internal: credit dst only
      - external->external: ignored
    """
    amt = float(amount)
    if amt <= 0.0 or not np.isfinite(amt):
        return False

    src_ext = is_external_account(src)
    dst_ext = is_external_account(dst)

    balances = book.balances
    overdraft = book.overdraft_limit

    # external -> external
    if src_ext and dst_ext:
        return False

    # external -> internal
    if src_ext and not dst_ext:
        di = book.acct_index.get(dst)
        if di is None:
            return False

        balances[di] = _scalar_at(balances, di) + amt
        return True

    # internal -> external
    if not src_ext and dst_ext:
        si = book.acct_index.get(src)
        if si is None:
            return False

        if si in book.hub_set_idx:
            return True

        bal = _scalar_at(balances, si)
        limit = _scalar_at(overdraft, si)

        if bal + limit < amt:
            return False

        balances[si] = bal - amt
        return True

    # internal -> internal
    si = book.acct_index.get(src)
    di = book.acct_index.get(dst)
    if si is None or di is None:
        return False

    if si in book.hub_set_idx:
        balances[di] = _scalar_at(balances, di) + amt
        return True

    bal = _scalar_at(balances, si)
    limit = _scalar_at(overdraft, si)

    if bal + limit < amt:
        return False

    balances[si] = bal - amt
    balances[di] = _scalar_at(balances, di) + amt
    return True
