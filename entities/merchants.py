from dataclasses import dataclass
from typing import cast

import numpy as np

from common.config import MerchantsConfig, PopulationConfig
from common.ids import external_account_id, merchant_account_id
from common.math import as_float, NumScalar, ArrF64
from common.random import Rng, derived_seed


@dataclass(frozen=True, slots=True)
class MerchantData:
    merchant_ids: list[str]
    counterparty_accts: list[str]
    categories: list[str]
    weights: ArrF64

    in_bank_accounts: list[str]
    external_accounts: list[str]


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


def _create_merchant_id(i: int) -> str:
    return f"MERCH{i:06d}"


def generate_merchants(
    mcfg: MerchantsConfig,
    pop: PopulationConfig,
    rng: Rng,
) -> MerchantData:
    num_people = int(pop.size)

    num_merchants = int(round(float(mcfg.per_10k_people) * (num_people / 10_000.0)))
    num_merchants = max(50, num_merchants)

    sigma = float(mcfg.size_lognormal_sigma)

    # Lognormal distribution creates a realistic heavy-tail of merchant traffic
    base_weights = rng.gen.lognormal(mean=0.0, sigma=sigma, size=num_merchants)

    weights_obj = cast(object, base_weights)
    weights: ArrF64 = np.asarray(weights_obj, dtype=np.float64)
    weights_sum = as_float(cast(NumScalar, np.sum(weights, dtype=np.float64)))
    if not np.isfinite(weights_sum) or weights_sum <= 0.0:
        weights[:] = 1.0
        weights_sum = float(weights.size)

    weights = weights / weights_sum

    merchant_ids: list[str] = []
    counterparty_accts: list[str] = []
    categories: list[str] = []
    in_bank_accounts: list[str] = []
    external_accounts: list[str] = []

    # Updated to match our new _pct standard
    in_bank_pct = float(mcfg.in_bank_p)

    # Renamed g to local_rng
    local_rng = np.random.default_rng(
        derived_seed(int(pop.seed), "merchant_generation")
    )

    for i in range(1, num_merchants + 1):
        merchant_ids.append(_create_merchant_id(i))

        cat_idx = int(cast(int | np.integer, local_rng.integers(0, len(_CATEGORIES))))
        cat_name = _CATEGORIES[cat_idx]
        categories.append(cat_name)

        if float(local_rng.random()) < in_bank_pct:
            acct_id = merchant_account_id(i)
            in_bank_accounts.append(acct_id)
        else:
            acct_id = external_account_id(i)
            external_accounts.append(acct_id)

        counterparty_accts.append(acct_id)

    return MerchantData(
        merchant_ids=merchant_ids,
        counterparty_accts=counterparty_accts,
        categories=categories,
        weights=weights,
        in_bank_accounts=in_bank_accounts,
        external_accounts=external_accounts,
    )
