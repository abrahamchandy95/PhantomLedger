from dataclasses import dataclass, field
from typing import Literal

from common.random import Rng
from common.validate import (
    require_float_between,
    require_float_gt,
    require_int_ge,
)

type Typology = Literal["classic", "layering", "funnel", "structuring", "invoice"]


@dataclass(frozen=True, slots=True)
class TypologyMixPolicy:
    classic_weight: float = 0.45
    layering_weight: float = 0.20
    funnel_weight: float = 0.15
    structuring_weight: float = 0.15
    invoice_weight: float = 0.05

    def validate(self) -> None:
        weights = (
            self.classic_weight,
            self.layering_weight,
            self.funnel_weight,
            self.structuring_weight,
            self.invoice_weight,
        )
        if any(weight < 0.0 for weight in weights):
            raise ValueError("fraud typology weights must be >= 0")

    def choose(self, rng: Rng) -> Typology:
        items: tuple[Typology, ...] = (
            "classic",
            "layering",
            "funnel",
            "structuring",
            "invoice",
        )
        weights = (
            float(self.classic_weight),
            float(self.layering_weight),
            float(self.funnel_weight),
            float(self.structuring_weight),
            float(self.invoice_weight),
        )
        if float(sum(weights)) <= 0.0:
            return "classic"
        return rng.weighted_choice(items, weights)


@dataclass(frozen=True, slots=True)
class LayeringPolicy:
    min_hops: int = 3
    max_hops: int = 8

    def validate(self) -> None:
        require_int_ge("min_hops", self.min_hops, 1)
        require_int_ge("max_hops", self.max_hops, self.min_hops)


@dataclass(frozen=True, slots=True)
class StructuringPolicy:
    threshold: float = 10_000.0
    epsilon_min: float = 50.0
    epsilon_max: float = 400.0
    splits_min: int = 3
    splits_max: int = 12

    def validate(self) -> None:
        require_float_gt("threshold", self.threshold, 0.0)
        if float(self.epsilon_max) < float(self.epsilon_min):
            raise ValueError("epsilon_max must be >= epsilon_min")
        require_int_ge("splits_min", self.splits_min, 1)
        require_int_ge("splits_max", self.splits_max, self.splits_min)


@dataclass(frozen=True, slots=True)
class CamouflagePolicy:
    small_p2p_per_day_p: float = 0.03
    bill_monthly_p: float = 0.35
    salary_inbound_p: float = 0.12

    def validate(self) -> None:
        require_float_between("small_p2p_per_day_p", self.small_p2p_per_day_p, 0.0, 1.0)
        require_float_between("bill_monthly_p", self.bill_monthly_p, 0.0, 1.0)
        require_float_between("salary_inbound_p", self.salary_inbound_p, 0.0, 1.0)


@dataclass(frozen=True, slots=True)
class FraudPolicies:
    typology: TypologyMixPolicy = field(default_factory=TypologyMixPolicy)
    layering: LayeringPolicy = field(default_factory=LayeringPolicy)
    structuring: StructuringPolicy = field(default_factory=StructuringPolicy)
    camouflage: CamouflagePolicy = field(default_factory=CamouflagePolicy)

    def validate(self) -> None:
        self.typology.validate()
        self.layering.validate()
        self.structuring.validate()
        self.camouflage.validate()
