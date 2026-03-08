from dataclasses import dataclass

from common.validate import (
    require_float_between,
    require_float_gt,
    require_int_gt,
)


@dataclass(frozen=True, slots=True)
class GraphConfig:
    graph_k_neighbors: int = 12
    graph_intra_household_p: float = 0.70
    graph_hub_weight_boost: float = 25.0
    graph_attractiveness_sigma: float = 1.1
    graph_edge_weight_gamma_shape: float = 1.0

    def validate(self) -> None:
        require_int_gt("graph_k_neighbors", self.graph_k_neighbors, 0)
        require_float_between(
            "graph_intra_household_p",
            self.graph_intra_household_p,
            0.0,
            1.0,
        )
        require_float_gt("graph_hub_weight_boost", self.graph_hub_weight_boost, 0.0)
        require_float_gt(
            "graph_attractiveness_sigma",
            self.graph_attractiveness_sigma,
            0.0,
        )
        require_float_gt(
            "graph_edge_weight_gamma_shape",
            self.graph_edge_weight_gamma_shape,
            0.0,
        )
