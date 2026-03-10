from dataclasses import dataclass, field
from datetime import datetime

from common.config import EventsConfig, MerchantsConfig
from common.math import ArrF32, ArrF64, ArrI16, ArrI32
from common.random import Rng
from entities.credit_cards import CreditIssuancePolicy
from entities.merchants import MerchantData
from infra.txn_infra import TxnInfraAssigner
from math_models.timing import TimingProfiles
from relationships.social import SocialGraph


@dataclass(frozen=True, slots=True)
class DayToDayPolicy:
    biller_categories: tuple[str, ...] = (
        "utilities",
        "telecom",
        "insurance",
        "education",
    )

    unique_pick_max_tries: int = 250
    merchant_retry_limit: int = 6

    burst_probability: float = 0.08
    burst_len_min: int = 3
    burst_len_max: int = 9

    explore_beta_alpha: float = 1.6
    explore_beta_beta: float = 9.5

    weekend_explore_multiplier: float = 1.25
    burst_explore_multiplier: float = 3.25

    def validate(self) -> None:
        from common.validate import (
            require_float_between,
            require_float_gt,
            require_int_ge,
        )

        require_int_ge("unique_pick_max_tries", self.unique_pick_max_tries, 1)
        require_int_ge("merchant_retry_limit", self.merchant_retry_limit, 0)

        require_float_between("burst_probability", self.burst_probability, 0.0, 1.0)
        require_int_ge("burst_len_min", self.burst_len_min, 1)
        require_int_ge("burst_len_max", self.burst_len_max, self.burst_len_min)

        require_float_gt("explore_beta_alpha", self.explore_beta_alpha, 0.0)
        require_float_gt("explore_beta_beta", self.explore_beta_beta, 0.0)

        require_float_gt(
            "weekend_explore_multiplier",
            self.weekend_explore_multiplier,
            0.0,
        )
        require_float_gt(
            "burst_explore_multiplier",
            self.burst_explore_multiplier,
            0.0,
        )


DEFAULT_DAY_TO_DAY_POLICY = DayToDayPolicy()


@dataclass(frozen=True, slots=True)
class DayToDayContext:
    persons: list[str]
    people_index: dict[str, int]

    primary_acct_for_person: dict[str, str]
    persona_for_person: dict[str, str]

    timing: TimingProfiles

    merchants: MerchantData
    merch_cdf: ArrF64
    global_biller_cdf: ArrF64

    social: SocialGraph

    fav_merchants_idx: ArrI32
    fav_k: ArrI16

    billers_idx: ArrI32
    bill_k: ArrI16

    explore_propensity: ArrF32
    burst_start_day: ArrI32
    burst_len: ArrI16


@dataclass(frozen=True, slots=True)
class DayToDayBuildRequest:
    events: EventsConfig
    merchants_cfg: MerchantsConfig
    rng: Rng
    start_date: datetime
    days: int
    persons: list[str]
    primary_acct_for_person: dict[str, str]
    persona_for_person: dict[str, str]
    merchants: MerchantData
    social: SocialGraph
    base_seed: int
    policy: DayToDayPolicy = field(default_factory=DayToDayPolicy)


@dataclass(frozen=True, slots=True)
class DayToDayGenerationRequest:
    events: EventsConfig
    merchants_cfg: MerchantsConfig
    rng: Rng
    start_date: datetime
    days: int
    ctx: DayToDayContext
    infra: TxnInfraAssigner | None = None
    credit_policy: CreditIssuancePolicy | None = None
    card_for_person: dict[str, str] | None = None
    policy: DayToDayPolicy = field(default_factory=DayToDayPolicy)
