from dataclasses import dataclass
from math import log
from typing import cast

import numpy as np

from common.config import BalancesConfig
from common.rng import Rng
from common.ids import is_external_account


@dataclass(slots=True)
class BalanceBook:
    balances: np.ndarray
    overdraft_limit: np.ndarray
    acct_index: dict[str, int]
    hub_set_idx: set[int]


_NumScalar = float | int | np.floating | np.integer


def _as_float(x: object) -> float:
    return float(cast(_NumScalar, x))


def _lognormal_by_median(
    rng: Rng, median: float, sigma: float, size: int
) -> np.ndarray:
    # Use math.log to avoid NumPy typing Any on np.log(...)
    mu = log(max(1e-12, float(median)))

    out_obj: object = cast(
        object, rng.gen.lognormal(mean=mu, sigma=float(sigma), size=size)
    )
    return np.asarray(out_obj, dtype=np.float64)


def init_balances(
    bcfg: BalancesConfig,
    rng: Rng,
    *,
    accounts: list[str],
    acct_index: dict[str, int],
    hub_set_idx: set[int],
    persona_for_acct: np.ndarray,
    persona_names: list[str],
) -> BalanceBook:
    """
    Initialize account balances and overdraft limits.

    Depends only on BalancesConfig (SRP): no access to unrelated generation settings.
    """
    n = len(accounts)
    balances = np.zeros(n, dtype=np.float64)

    sigma = float(bcfg.init_bal_sigma)
    medians: dict[str, float] = {
        "student": float(bcfg.init_bal_student),
        "salaried": float(bcfg.init_bal_salaried),
        "retired": float(bcfg.init_bal_retired),
        "freelancer": float(bcfg.init_bal_freelancer),
        "smallbiz": float(bcfg.init_bal_smallbiz),
        "hnw": float(bcfg.init_bal_hnw),
    }

    persona_obj: object = cast(object, persona_for_acct)
    persona_arr = np.asarray(persona_obj, dtype=np.int64)

    for pid, pname in enumerate(persona_names):
        median = medians.get(pname, medians["salaried"])

        mask_obj: object = cast(object, persona_arr == int(pid))
        mask = np.asarray(mask_obj, dtype=np.bool_)

        cnt = int(np.count_nonzero(mask))
        if cnt <= 0:
            continue

        balances[mask] = _lognormal_by_median(rng, median, sigma, cnt)

    # hubs get effectively infinite balance
    for i in hub_set_idx:
        balances[i] = 1e18

    overdraft_limit = np.zeros(n, dtype=np.float64)
    od_frac = float(bcfg.overdraft_frac)

    if od_frac > 0.0:
        elig = np.ones(n, dtype=np.bool_)
        for i in hub_set_idx:
            elig[i] = False

        elig_idx = np.nonzero(elig)[0]
        elig_count = int(elig_idx.size)
        k = int(od_frac * elig_count)

        if k > 0:
            chosen_obj: object = cast(
                object, rng.gen.choice(elig_idx, size=k, replace=False)
            )
            chosen = np.asarray(chosen_obj, dtype=np.int64)

            overdraft_limit[chosen] = _lognormal_by_median(
                rng,
                float(bcfg.overdraft_limit_median),
                float(bcfg.overdraft_limit_sigma),
                int(chosen.size),
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
      - internal->external (dst startswith X): debit src only
      - external->internal (src startswith X): credit dst only
      - external->external: ignored (returns False)
    """
    amt = float(amount)
    if amt <= 0.0 or not np.isfinite(amt):
        return False

    src_ext = is_external_account(src)
    dst_ext = is_external_account(dst)

    balances = book.balances
    overdraft = book.overdraft_limit

    # external -> external: ignore
    if src_ext and dst_ext:
        return False

    # external -> internal: credit only
    if src_ext and not dst_ext:
        di = book.acct_index.get(dst)
        if di is None:
            return False
        di_val_obj: object = cast(object, balances[di])
        balances[di] = _as_float(di_val_obj) + amt
        return True

    # internal -> external: debit only (respect overdraft unless hub)
    if not src_ext and dst_ext:
        si = book.acct_index.get(src)
        if si is None:
            return False

        # hubs effectively infinite
        if si in book.hub_set_idx:
            return True

        si_val_obj: object = cast(object, balances[si])
        od_val_obj: object = cast(object, overdraft[si])
        bal = _as_float(si_val_obj)
        limit = _as_float(od_val_obj)

        if bal + limit < amt:
            return False
        balances[si] = bal - amt
        return True

    # internal -> internal (original behavior)
    si = book.acct_index.get(src)
    di = book.acct_index.get(dst)
    if si is None or di is None:
        return False

    if si in book.hub_set_idx:
        di_val_obj2: object = cast(object, balances[di])
        balances[di] = _as_float(di_val_obj2) + amt
        return True

    si_val_obj2: object = cast(object, balances[si])
    od_val_obj2: object = cast(object, overdraft[si])
    bal2 = _as_float(si_val_obj2)
    limit2 = _as_float(od_val_obj2)

    if bal2 + limit2 < amt:
        return False

    balances[si] = bal2 - amt
    di_val_obj3: object = cast(object, balances[di])
    balances[di] = _as_float(di_val_obj3) + amt
    return True
