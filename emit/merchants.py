from typing import cast

import numpy as np

from common.math import ArrF64, NumScalar, as_float
from emit.csv_io import CsvCell, CsvRow
from entities.merchants import MerchantData


def iter_merchants_rows(data: MerchantData) -> list[CsvRow]:
    # merchants.csv: merchant_id, counterparty_acct, category, weight, in_bank
    rows: list[CsvRow] = []

    weights: ArrF64 = np.asarray(data.weights, dtype=np.float64).reshape(-1)

    for i, (merchant_id, acct, category) in enumerate(
        zip(data.merchant_ids, data.counterparty_accts, data.categories)
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
    for acct, category in zip(data.counterparty_accts, data.categories):
        if acct.startswith("X"):
            rows.append([acct, "merchant_external", category])
    return rows
