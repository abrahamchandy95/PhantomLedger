from dataclasses import dataclass
from enum import StrEnum
from pathlib import Path


class UseCase(StrEnum):
    STANDARD = "standard"
    MULE_ML = "mule-ml"
    AML = "aml"


@dataclass(frozen=True, slots=True)
class RunOptions:
    usecase: UseCase = UseCase.STANDARD
    out_dir: Path = Path("out_bank_data")
    show_transactions: bool = False
    progress: bool = False
    include_standard_export: bool = True
