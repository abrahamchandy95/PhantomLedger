from dataclasses import dataclass
from typing import cast

import numpy as np
import numpy.typing as npt

from common.config import MerchantsConfig, PopulationConfig
from common.ids import external_account_id, merchant_account_id
from common.probability import as_float
from common.rng import Rng
from common.seeding import derived_seed
from emit.tg_csv import CsvCell, CsvRow


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

    w_obj = cast(
        object,
        rng.gen.lognormal(mean=0.0, sigma=sigma, size=n_merchants),
    )
    w: ArrF64 = np.asarray(w_obj, dtype=np.float64)

    w_sum = as_float(cast(NumScalar, np.sum(w, dtype=np.float64)))
    if not np.isfinite(w_sum) or w_sum <= 0.0:
        w[:] = 1.0
        w_sum = float(w.size)

    w = w / w_sum

    merchant_ids: list[str] = []
    counterparty_acct: list[str] = []
    category: list[str] = []
    in_bank_accounts: list[str] = []
    external_accounts: list[str] = []

    in_bank_frac = float(mcfg.in_bank_merchant_frac)

    g = np.random.default_rng(derived_seed(int(pop.seed), "merchant_generation"))

    for i in range(1, n_merchants + 1):
        merchant_ids.append(_merchant_entity_id(i))

        cat_idx = int(cast(int | np.integer, g.integers(0, len(_CATEGORIES))))
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

    w_arr: ArrF64 = np.asarray(data.weight, dtype=np.float64).reshape(-1)

    for i, (mid, acct, cat) in enumerate(
        zip(data.merchant_ids, data.counterparty_acct, data.category)
    ):
        w = as_float(cast(NumScalar, w_arr[i]))
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
