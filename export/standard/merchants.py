from collections.abc import Iterator
from typing import cast

from entities import models
from ..csv_io import Row


def merchant(data: models.Merchants) -> Iterator[Row]:
    """
    Yields rows for the MERCHANT vertex table.
    (merchant_id, counterparty_acct, category, weight, in_bank)
    """
    weights = cast(list[float], data.weights.tolist())

    for m_id, acct, category, weight in zip(
        data.ids,
        data.counterparties,
        data.categories,
        weights,
        strict=True,
    ):
        yield (
            m_id,
            acct,
            category,
            round(weight, 10),
            int(acct.startswith("M")),
        )
