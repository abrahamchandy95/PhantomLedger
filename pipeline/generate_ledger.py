from common.config import World as config_world
from common.progress import status
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

    status("Building entities...")
    entities = build_entities(cfg, rng)
    status("Building infrastructure...")
    infra = build_infra(cfg, rng, entities)
    status("Generating transfers...")
    transfers = build_transfers(cfg, rng, entities, infra)
    status("Simulation finished. Exporting files...")

    return SimulationResult(
        entities=entities,
        infra=infra,
        transfers=transfers,
    )
