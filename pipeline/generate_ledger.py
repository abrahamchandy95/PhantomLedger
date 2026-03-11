from pathlib import Path

from common.config import GenerationConfig
from common.random import Rng
from pipeline.state import GenerationRuntime, GenerationState
from pipeline.stages import (
    build_entities,
    build_infra,
    build_transfers,
    emit_outputs,
    print_summary,
)


def generate_ledger(config: GenerationConfig | None = None) -> None:
    if config is None:
        config = GenerationConfig.create_default()
    else:
        config.validate()
    output_dir: Path = config.output.out_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    rng = Rng.from_seed(config.population.seed)

    state = GenerationState(
        runtime=GenerationRuntime(
            cfg=config,
            rng=rng,
            out_dir=output_dir,
        )
    )

    # 3. Pipeline Execution
    build_entities(state)
    build_infra(state)
    build_transfers(state)
    emit_outputs(state)
    print_summary(state)
