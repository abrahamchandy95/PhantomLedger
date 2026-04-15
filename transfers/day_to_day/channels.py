from typing import cast
import numpy as np

from common import config
from common.math import F64, Scalar, as_float, build_cdf


def build_channel_cdf(
    events_cfg: config.Events, merchants_cfg: config.Merchants
) -> F64:
    unknown_p = min(1.0, max(0.0, float(events_cfg.unknown_outflow_p)))

    core = np.array(
        [
            float(merchants_cfg.channel_merchant_p),
            float(merchants_cfg.channel_bills_p),
            float(merchants_cfg.channel_p2p_p),
        ],
        dtype=np.float64,
    )

    core_sum = as_float(cast(Scalar, np.sum(core, dtype=np.float64)))
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
