from typing import cast

import numpy as np
import numpy.typing as npt

from common.probability import as_float
from emit.csv_io import CsvCell, CsvRow
from entities.merchants import MerchantData


type NumScalar = float | int | np.floating | np.integer
type ArrF64 = npt.NDArray[np.float64]


def iter_merchants_rows(data: MerchantData) -> list[CsvRow]:
    # merchants.csv: merchant_id, counterparty_acct, category, weight, in_bank
    rows: list[CsvRow] = []

    weights: ArrF64 = np.asarray(data.weight, dtype=np.float64).reshape(-1)

    for i, (merchant_id, acct, category) in enumerate(
        zip(data.merchant_ids, data.counterparty_acct, data.category)
    ):
        weight = as_float(cast(NumScalar, weights[i]))
        row: list[CsvCell] = [
            merchant_id,
            acct,
            category,
            round(weight, 10),
            1 if acct.startswith("M") else 0,
        ]
        rows.append(row)

    return rows


def iter_external_accounts_rows(data: MerchantData) -> list[CsvRow]:
    # external_accounts.csv: account_id, kind, category
    rows: list[CsvRow] = []
    for acct, category in zip(data.counterparty_acct, data.category):
        if acct.startswith("X"):
            rows.append([acct, "merchant_external", category])
    return rows
