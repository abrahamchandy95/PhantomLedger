from pathlib import Path

from common.run import UseCase
from pipeline.result import SimulationResult
from export.registry import register


@register(UseCase.AML)
def export(
    result: SimulationResult,
    out_dir: Path,
    show_transactions: bool,
) -> None:
    raise NotImplementedError("AML exporter not yet implemented")
