from dataclasses import dataclass
from typing import cast

import numpy as np
import numpy.typing as npt

from common.config import MerchantsConfig, PopulationConfig
from common.ids import external_account_id, merchant_account_id
from common.rng import Rng
from common.seeding import derived_seed
from emit.tg_csv import CsvCell, CsvRow

# Python 3.12+ typing (works in 3.14)
type NumScalar = float | int | np.floating | np.integer
type ArrF64 = npt.NDArray[np.float64]


@dataclass(frozen=True, slots=True)
class MerchantData:
    merchant_ids: list[str]  # e.g. MERCH000001
    counterparty_acct: list[str]  # M... or X...
    category: list[str]
    weight: ArrF64  # float64 selection weights (sum ~ 1)

    in_bank_accounts: list[str]  # subset starting with M
    external_accounts: list[str]  # subset starting with X


_CATEGORIES: tuple[str, ...] = (
    "grocery",
    "fuel",
    "utilities",
    "telecom",
    "ecommerce",
    "restaurant",
    "pharmacy",
    "retail_other",
    "insurance",
    "education",
)


def _merchant_entity_id(i: int) -> str:
    return f"MERCH{i:06d}"


def _as_float(x: object) -> float:
    return float(cast(NumScalar, x))


def generate_merchants(
    mcfg: MerchantsConfig,
    pop: PopulationConfig,
    rng: Rng,
) -> MerchantData:
    n_people = int(pop.persons)

    n_merchants = int(
        round(float(mcfg.merchants_per_10k_people) * (n_people / 10_000.0))
    )
    n_merchants = max(50, n_merchants)

    sigma = float(mcfg.size_lognormal_sigma)

    # Avoid Any leakage from NumPy stubs by casting through object and re-typing.
    w_obj: object = cast(
        object, rng.gen.lognormal(mean=0.0, sigma=sigma, size=n_merchants)
    )
    w: ArrF64 = np.asarray(w_obj, dtype=np.float64)

    s_obj: object = cast(object, np.sum(w, dtype=np.float64))
    s = _as_float(s_obj)

    if not np.isfinite(s) or s <= 0.0:
        w[:] = 1.0
        s = float(w.size)

    w = w / s

    merchant_ids: list[str] = []
    counterparty_acct: list[str] = []
    category: list[str] = []
    in_bank_accounts: list[str] = []
    external_accounts: list[str] = []

    in_bank_frac = float(mcfg.in_bank_merchant_frac)

    g = np.random.default_rng(derived_seed(int(pop.seed), "merchant_generation"))

    for i in range(1, n_merchants + 1):
        merchant_ids.append(_merchant_entity_id(i))

        cat_idx_obj: object = cast(object, g.integers(0, len(_CATEGORIES)))
        cat_idx = int(cast(int | np.integer, cat_idx_obj))
        cat = _CATEGORIES[cat_idx]
        category.append(cat)

        if float(g.random()) < in_bank_frac:
            acct = merchant_account_id(i)
            in_bank_accounts.append(acct)
        else:
            acct = external_account_id(i)
            external_accounts.append(acct)

        counterparty_acct.append(acct)

    return MerchantData(
        merchant_ids=merchant_ids,
        counterparty_acct=counterparty_acct,
        category=category,
        weight=w,
        in_bank_accounts=in_bank_accounts,
        external_accounts=external_accounts,
    )


def iter_merchants_rows(data: MerchantData) -> list[CsvRow]:
    # merchants.csv: merchant_id, counterparty_acct, category, weight, in_bank
    rows: list[CsvRow] = []

    w_arr = np.asarray(data.weight, dtype=np.float64).reshape(-1)

    for i, (mid, acct, cat) in enumerate(
        zip(data.merchant_ids, data.counterparty_acct, data.category)
    ):
        w = _as_float(cast(object, w_arr[i]))
        row: list[CsvCell] = [
            mid,
            acct,
            cat,
            round(w, 10),
            1 if acct.startswith("M") else 0,
        ]
        rows.append(row)

    return rows


def iter_external_accounts_rows(data: MerchantData) -> list[CsvRow]:
    # external_accounts.csv: account_id, kind, category
    rows: list[CsvRow] = []
    for acct, cat in zip(data.counterparty_acct, data.category):
        if acct.startswith("X"):
            rows.append([acct, "merchant_external", cat])
    return rows
