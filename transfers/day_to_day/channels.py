from typing import cast

import numpy as np

from common.math import ArrF64, NumScalar, as_float, build_cdf
from common.transactions import Transaction

from .models import DayToDayGenerationRequest
from .payments import (
    emit_bill_txn,
    emit_external_unknown_txn,
    emit_merchant_txn,
    emit_p2p_txn,
)
from .state import EventFrame


def build_channel_cdf(
    request: DayToDayGenerationRequest,
) -> ArrF64:
    unknown_p = min(1.0, max(0.0, float(request.events.unknown_outflow_p)))

    core = np.array(
        [
            float(request.merchants_cfg.channel_merchant_p),
            float(request.merchants_cfg.channel_bills_p),
            float(request.merchants_cfg.channel_p2p_p),
        ],
        dtype=np.float64,
    )

    core_sum = as_float(cast(NumScalar, np.sum(core, dtype=np.float64)))
    if not np.isfinite(core_sum) or core_sum <= 0.0:
        core[:] = 1.0
        core_sum = float(core.size)

    core = core / core_sum

    shares = np.array(
        [
            (1.0 - unknown_p) * core[0],
            (1.0 - unknown_p) * core[1],
            (1.0 - unknown_p) * core[2],
            unknown_p,
        ],
        dtype=np.float64,
    )

    return build_cdf(shares)


def build_channel_txn(
    channel_idx: int,
    request: DayToDayGenerationRequest,
    event_frame: EventFrame,
    prefer_billers_p: float,
) -> Transaction | None:
    if channel_idx == 2:
        return emit_p2p_txn(request, event_frame)
    if channel_idx == 1:
        return emit_bill_txn(request, event_frame, prefer_billers_p)
    if channel_idx == 3:
        return emit_external_unknown_txn(request, event_frame)
    return emit_merchant_txn(request, event_frame)
