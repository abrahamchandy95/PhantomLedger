"""
Aggregated configuration for all per-person behavioral dynamics.

Each sub-config lives with its own math_models module. This dataclass
groups them so callers pass one object instead of four.
"""

from dataclasses import dataclass, field

from math_models.momentum import MomentumConfig
from math_models.dormancy import DormancyConfig
from math_models.paycheck_cycle import PaycheckCycleConfig
from math_models.counterparty_evolution import EvolutionConfig


@dataclass(frozen=True, slots=True)
class DynamicsConfig:
    momentum: MomentumConfig = field(default_factory=MomentumConfig)
    dormancy: DormancyConfig = field(default_factory=DormancyConfig)
    paycheck: PaycheckCycleConfig = field(default_factory=PaycheckCycleConfig)
    evolution: EvolutionConfig = field(default_factory=EvolutionConfig)


DEFAULT_DYNAMICS_CONFIG = DynamicsConfig()
