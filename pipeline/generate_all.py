from pathlib import Path

from common.config import GenerationConfig, default_config
from common.random import Rng
from pipeline.state import GenerationRuntime, GenerationState
from pipeline.stages import (
    build_entities,
    build_infra,
    build_transfers,
    emit_outputs,
    print_summary,
)


def generate_all(cfg: GenerationConfig | None = None) -> None:
    if cfg is None:
        cfg = default_config()
    else:
        cfg.validate()

    out_dir: Path = cfg.output.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    rng = Rng.from_seed(cfg.population.seed)
    st = GenerationState(
        runtime=GenerationRuntime(
            cfg=cfg,
            rng=rng,
            out_dir=out_dir,
        )
    )

    build_entities(st)
    build_infra(st)
    build_transfers(st)
    emit_outputs(st)
    print_summary(st)
