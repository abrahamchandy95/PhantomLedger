from pathlib import Path
from collections.abc import Callable

from common.run import UseCase

from pipeline.result import SimulationResult

type Exporter = Callable[[SimulationResult, Path, bool, bool], None]

_REGISTRY: dict[UseCase, Exporter] = {}


def register(usecase: UseCase) -> Callable[[Exporter], Exporter]:
    def decorator(fn: Exporter) -> Exporter:
        _REGISTRY[usecase] = fn
        return fn

    return decorator


def export(
    usecase: UseCase,
    result: SimulationResult,
    out_dir: Path,
    show_transactions: bool = False,
    include_standard_export: bool = True,
) -> None:
    fn = _REGISTRY.get(usecase)
    if fn is None:
        available = [uc.value for uc in _REGISTRY]
        raise ValueError(f"No exporter for '{usecase}'. Available: {available}")
    out_dir.mkdir(parents=True, exist_ok=True)
    fn(result, out_dir, show_transactions, include_standard_export)
