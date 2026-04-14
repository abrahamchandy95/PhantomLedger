from pathlib import Path

from common.run import UseCase
from pipeline.result import SimulationResult
from export.registry import register


@register(UseCase.AML)
def export(
    result: SimulationResult,
    out_dir: Path,
    show_transactions: bool,
    include_standard_export: bool = True,
) -> None:
    _ = (result, out_dir, show_transactions, include_standard_export)
    raise NotImplementedError("AML exporter not yet implemented")
