from .burdens import monthly_fixed_burden_for_portfolio
from .limits import build_balance_book
from .posting import (
    ChronoReplayAccumulator,
    DEFAULT_REPLAY_POLICY,
    ReplayPolicy,
    merge_replay_sorted,
    sort_for_replay,
)

__all__ = [
    "monthly_fixed_burden_for_portfolio",
    "build_balance_book",
    "ChronoReplayAccumulator",
    "DEFAULT_REPLAY_POLICY",
    "ReplayPolicy",
    "merge_replay_sorted",
    "sort_for_replay",
]


def __getattr__(name: str):
    if name == "LegitTransferBuilder":
        from .builder import LegitTransferBuilder

        return LegitTransferBuilder
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
