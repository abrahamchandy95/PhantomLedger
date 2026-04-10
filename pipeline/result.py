from dataclasses import dataclass

from pipeline.state import Entities, Infra, Transfers


@dataclass(frozen=True, slots=True)
class SimulationResult:
    """Canonical output of the simulation pipeline. Exporters consume this."""

    entities: Entities
    infra: Infra
    transfers: Transfers
