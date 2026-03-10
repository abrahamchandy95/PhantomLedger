from datetime import timedelta

import numpy as np

from common.random import derived_seed
from common.temporal import iter_window_month_starts
from common.types import Txn

from .allowances import generate_allowance_txns
from .models import FamilyTransferRequest, FamilyTransferSchedule
from .support import generate_support_txns
from .tuition import generate_tuition_txns


def _build_schedule(request: FamilyTransferRequest) -> FamilyTransferSchedule:
    start_date = request.window.start_date()
    days = int(request.window.days)
    end_excl = start_date + timedelta(days=days)

    month_starts = iter_window_month_starts(start_date, days)
    if not month_starts:
        month_starts = [start_date]

    return FamilyTransferSchedule(
        start_date=start_date,
        end_excl=end_excl,
        month_starts=month_starts,
    )


def generate_family_transfers(
    request: FamilyTransferRequest,
) -> list[Txn]:
    schedule = _build_schedule(request)

    gen = np.random.default_rng(derived_seed(request.base_seed, "family", "transfers"))

    txns: list[Txn] = []
    txns.extend(generate_allowance_txns(request, schedule, gen))
    txns.extend(generate_tuition_txns(request, schedule, gen))
    txns.extend(generate_support_txns(request, schedule, gen))
    return txns
