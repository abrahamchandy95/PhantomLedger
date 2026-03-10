from dataclasses import dataclass
from typing import cast

import numpy as np

from common.config import MerchantsConfig, PopulationConfig
from common.ids import external_account_id, merchant_account_id
from common.math import as_float, NumScalar, ArrF64
from common.random import Rng, derived_seed


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
