from common.config import World as config_world
from common.random import Rng
from pipeline.result import SimulationResult
from pipeline.stages import (
    build_entities,
    build_infra,
    build_transfers,
)


def simulate(cfg: config_world) -> SimulationResult:
    """
    Main orchestration entry point for the synthetic ledger generation.
    """
    rng = Rng.from_seed(cfg.population.seed)

    entities = build_entities(cfg, rng)
    infra = build_infra(cfg, rng, entities)
    transfers = build_transfers(cfg, rng, entities, infra)

    return SimulationResult(
        entities=entities,
        infra=infra,
        transfers=transfers,
    )
