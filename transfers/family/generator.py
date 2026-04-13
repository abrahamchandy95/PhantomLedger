from datetime import timedelta

import numpy as np

from common.random import derive_seed
from common.timeline import active_months
from common.transactions import Transaction

from .allowances import generate as generate_allowances
from .engine import Runtime, Schedule, TransferConfig
from .gifts import generate as generate_gifts
from .grandparent import generate as generate_grandparent
from .inheritance import generate as generate_inheritance
from .siblings import generate as generate_siblings
from .spouse import generate as generate_spouse
from .support import generate as generate_support
from .tuition import generate as generate_tuition


def _build_schedule(request: Runtime) -> Schedule:
    start_date = request.window.start_date
    days = int(request.window.days)
    end_excl = start_date + timedelta(days=days)

    month_starts = active_months(start_date, days)
    if not month_starts:
        month_starts = [start_date]

    return Schedule(
        start_date=start_date,
        end_excl=end_excl,
        month_starts=month_starts,
    )


def generate(rt: Runtime, cfg: TransferConfig) -> list[Transaction]:
    """Entry point for generating all internal family financial transfers."""
    schedule = _build_schedule(rt)

    gen = np.random.default_rng(derive_seed(rt.base_seed, "family", "transfers"))

    txns: list[Transaction] = []

    txns.extend(
        generate_allowances(
            rt,
            cfg.allowances,
            cfg.routing,
            schedule,
            gen,
        )
    )
    txns.extend(
        generate_tuition(
            rt,
            cfg.tuition,
            schedule,
            gen,
        )
    )
    txns.extend(
        generate_support(
            rt,
            cfg.retiree_support,
            cfg.routing,
            schedule,
            gen,
        )
    )
    txns.extend(
        generate_spouse(
            rt,
            cfg.spouses,
            cfg.routing,
            schedule,
            gen,
        )
    )
    txns.extend(
        generate_gifts(
            rt,
            cfg.parent_gifts,
            cfg.routing,
            schedule,
            gen,
        )
    )
    txns.extend(
        generate_siblings(
            rt,
            cfg.sibling_transfers,
            cfg.routing,
            schedule,
            gen,
        )
    )
    txns.extend(
        generate_grandparent(
            rt,
            cfg.grandparent_gifts,
            schedule,
            gen,
        )
    )
    txns.extend(
        generate_inheritance(
            rt,
            cfg.inheritance,
            cfg.routing,
            schedule,
            gen,
        )
    )

    return txns
