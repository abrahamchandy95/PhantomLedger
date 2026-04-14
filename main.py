import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import cast

import common.config as config
from common import progress as progress_ui
from common.run import RunOptions, UseCase
from pipeline.generate_ledger import simulate

import export


@dataclass(frozen=True, slots=True)
class CLIArgs:
    usecase: UseCase
    days: int
    out_dir: Path
    show_transactions: bool
    progress: bool
    ml_only: bool


def parse_args() -> CLIArgs:
    p = argparse.ArgumentParser(
        description="PhantomLedger — synthetic bank transaction generator",
    )
    _ = p.add_argument(
        "--usecase",
        type=str,
        default="standard",
        choices=[uc.value for uc in UseCase],
    )
    _ = p.add_argument("--days", type=int, default=365)
    _ = p.add_argument("--out", type=str, default="out_bank_data")
    _ = p.add_argument("--show-transactions", action="store_true", default=False)
    _ = p.add_argument(
        "--progress",
        action="store_true",
        default=False,
        help="Show tqdm progress bars for long-running stages.",
    )
    _ = p.add_argument(
        "--ml-only",
        action="store_true",
        default=False,
        help="When --usecase mule-ml is selected, skip the standard export tables.",
    )

    raw = p.parse_args()

    return CLIArgs(
        usecase=UseCase(cast(str, raw.usecase)),
        days=cast(int, raw.days),
        out_dir=Path(cast(str, raw.out)),
        show_transactions=cast(bool, raw.show_transactions),
        progress=cast(bool, raw.progress),
        ml_only=cast(bool, raw.ml_only),
    )


def main() -> None:
    export.register_all()
    args = parse_args()
    progress_ui.enable(args.progress)

    world = config.World(
        window=config.Window(days=args.days),
    )

    opts = RunOptions(
        usecase=args.usecase,
        out_dir=args.out_dir,
        show_transactions=args.show_transactions,
        progress=args.progress,
        include_standard_export=not args.ml_only,
    )

    result = simulate(world)
    export.export(
        opts.usecase,
        result,
        opts.out_dir,
        opts.show_transactions,
        include_standard_export=opts.include_standard_export,
    )

    total = len(result.transfers.final_txns)
    illicit = sum(1 for t in result.transfers.final_txns if t.fraud_flag == 1)
    ratio = illicit / max(1, total)

    print(
        f"People: {len(result.entities.people.ids)}  Accounts: {len(result.entities.accounts.ids)}"
    )
    print(f"Transactions: {total}  Illicit: {illicit} ({ratio:.4%})")
    print(f"Output: {opts.out_dir}/")


if __name__ == "__main__":
    main()
