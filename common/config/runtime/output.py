from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True, slots=True)
class OutputConfig:
    out_dir: Path = Path("out_bank_data")
    emit_raw_ledger: bool = False
