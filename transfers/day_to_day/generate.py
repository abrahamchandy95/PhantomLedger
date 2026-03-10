from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

import numpy as np

from common.math import (
    ArrF64,
    ArrI32,
    NumScalar,
    as_float,
    as_int,
    build_cdf,
    cdf_pick,
)
from common.types import Txn
from entities.personas import PERSONAS, PersonaSpec
from math_models.amounts import bill_amount, p2p_amount
from math_models.counts import (
    DEFAULT_COUNT_MODELS,
    gamma_poisson_out_count,
    weekday_multiplier,
)
from math_models.timing import sample_offsets_seconds
from transfers.txns import TxnFactory, TxnSpec

from .models import DayToDayContext, DayToDayGenerationRequest


@dataclass(frozen=True, slots=True)
class _CreditRuntime:
    policy: object | None
    card_for_person: dict[str, str] | None

    def enabled(self) -> bool:
        return bool(
            self.policy is not None
            and self.card_for_person is not None
            and self.card_for_person
        )


@dataclass(frozen=True, slots=True)
class _PersonState:
    person_id: str
    deposit_acct: str
    persona_name: str
    persona: PersonaSpec
    favorite_k: int
    biller_k: int
    explore_propensity: float
    burst_start: int
    burst_len: int


@dataclass(frozen=True, slots=True)
class _DayFrame:
    day_index: int
    day_start: datetime
    is_weekend: bool
    day_shock: float


@dataclass(frozen=True, slots=True)
class _EventFrame:
    person_idx: int
    person_state: _PersonState
    txn_ts: datetime
    txf: TxnFactory
    explore_p: float


def _row_contains(row: ArrI32, k: int, value: int) -> bool:
    for idx in range(k):
        if as_int(cast(int | np.integer, row[idx])) == value:
            return True
    return False


def _build_channel_cdf(
    request: DayToDayGenerationRequest,
) -> ArrF64:
    unknown_p = min(1.0, max(0.0, float(request.events.unknown_outflow_p)))

    core = np.array(
        [
            float(request.merchants_cfg.share_merchant),
            float(request.merchants_cfg.share_bills),
            float(request.merchants_cfg.share_p2p),
        ],
        dtype=np.float64,
    )

    core_sum = as_float(cast(NumScalar, np.sum(core, dtype=np.float64)))
    if not np.isfinite(core_sum) or core_sum <= 0.0:
        core[:] = 1.0
        core_sum = float(core.size)

    core = core / core_sum

    shares = np.array(
        [
            (1.0 - unknown_p) * core[0],
            (1.0 - unknown_p) * core[1],
            (1.0 - unknown_p) * core[2],
            unknown_p,
        ],
        dtype=np.float64,
    )

    return build_cdf(shares)


@dataclass(slots=True)
class _DayToDayGenerator:
    request: DayToDayGenerationRequest

    def generate(self) -> list[Txn]:
        if self.request.days <= 0:
            return []

        self.request.policy.validate()

        txf = TxnFactory(rng=self.request.rng, infra=self.request.infra)
        channel_cdf = _build_channel_cdf(self.request)

        base_per_day = (
            float(self.request.merchants_cfg.target_payments_per_person_per_month)
            / 30.0
        )
        prefer_billers_p = float(self.request.events.prefer_billers_p)
        day_cap = int(self.request.events.max_events_per_day)

        txns: list[Txn] = []
        n_people = len(self.request.ctx.persons)

        for day_index in range(self.request.days):
            day_frame = self._day_frame(day_index)
            produced_today = 0

            for person_idx in range(n_people):
                if day_cap > 0 and produced_today >= day_cap:
                    break

                person_state = self._person_state(person_idx)
                if person_state is None:
                    continue

                n_out = self._n_out(
                    person_state,
                    day_frame,
                    base_per_day,
                    day_cap,
                    produced_today,
                )
                if n_out <= 0:
                    continue

                offsets: ArrI32 = np.asarray(
                    sample_offsets_seconds(
                        self.request.rng,
                        person_state.persona.timing_profile,
                        n_out,
                        self.request.ctx.timing,
                    ),
                    dtype=np.int32,
                )
                explore_p = self._explore_probability(person_state, day_frame)

                for event_idx in range(n_out):
                    offset_seconds = as_int(cast(int | np.integer, offsets[event_idx]))
                    event_frame = _EventFrame(
                        person_idx=person_idx,
                        person_state=person_state,
                        txn_ts=day_frame.day_start + timedelta(seconds=offset_seconds),
                        txf=txf,
                        explore_p=explore_p,
                    )

                    channel_idx = cdf_pick(channel_cdf, self.request.rng.float())
                    txn = self._txn_for_channel(
                        channel_idx,
                        event_frame,
                        prefer_billers_p,
                    )
                    if txn is None:
                        continue

                    txns.append(txn)
                    produced_today += 1

                    if day_cap > 0 and produced_today >= day_cap:
                        break

        return txns

    def _day_frame(self, day_index: int) -> _DayFrame:
        day_start = self.request.start_date + timedelta(days=day_index)
        is_weekend = day_start.weekday() >= 5

        gamma_shape = float(self.request.events.day_multiplier_gamma_shape)
        day_shock = float(
            self.request.rng.gen.gamma(shape=gamma_shape, scale=(1.0 / gamma_shape))
        )

        return _DayFrame(
            day_index=day_index,
            day_start=day_start,
            is_weekend=is_weekend,
            day_shock=day_shock,
        )

    def _person_state(self, person_idx: int) -> _PersonState | None:
        ctx: DayToDayContext = self.request.ctx

        person_id = ctx.persons[person_idx]
        deposit_acct = ctx.primary_acct_for_person.get(person_id)
        if deposit_acct is None:
            return None

        persona_name = ctx.persona_for_person.get(person_id, "salaried")
        persona = PERSONAS.get(persona_name, PERSONAS["salaried"])

        return _PersonState(
            person_id=person_id,
            deposit_acct=deposit_acct,
            persona_name=persona_name,
            persona=persona,
            favorite_k=as_int(cast(int | np.integer, ctx.fav_k[person_idx])),
            biller_k=as_int(cast(int | np.integer, ctx.bill_k[person_idx])),
            explore_propensity=as_float(
                cast(NumScalar, ctx.explore_propensity[person_idx])
            ),
            burst_start=as_int(cast(int | np.integer, ctx.burst_start_day[person_idx])),
            burst_len=as_int(cast(int | np.integer, ctx.burst_len[person_idx])),
        )

    def _n_out(
        self,
        person_state: _PersonState,
        day_frame: _DayFrame,
        base_per_day: float,
        day_cap: int,
        produced_today: int,
    ) -> int:
        rate = (
            base_per_day
            * float(person_state.persona.rate_mult)
            * float(weekday_multiplier(day_frame.day_start, DEFAULT_COUNT_MODELS))
            * day_frame.day_shock
        )

        n_out = gamma_poisson_out_count(
            self.request.rng,
            base_rate=rate,
            models=DEFAULT_COUNT_MODELS,
        )
        if n_out <= 0:
            return 0

        if day_cap > 0:
            n_out = min(n_out, max(0, day_cap - produced_today))
        return n_out

    def _explore_probability(
        self,
        person_state: _PersonState,
        day_frame: _DayFrame,
    ) -> float:
        explore_base = float(self.request.merchants_cfg.explore_p)
        policy = self.request.policy

        explore_p = explore_base * (0.25 + 0.75 * person_state.explore_propensity)

        if day_frame.is_weekend:
            explore_p *= float(policy.weekend_explore_multiplier)

        if (
            person_state.burst_start >= 0
            and person_state.burst_len > 0
            and (
                person_state.burst_start
                <= day_frame.day_index
                < person_state.burst_start + person_state.burst_len
            )
        ):
            explore_p *= float(policy.burst_explore_multiplier)

        return min(0.50, max(0.0, explore_p))

    def _txn_for_channel(
        self,
        channel_idx: int,
        event_frame: _EventFrame,
        prefer_billers_p: float,
    ) -> Txn | None:
        if channel_idx == 2:
            return self._emit_p2p(event_frame)
        if channel_idx == 1:
            return self._emit_bill(event_frame, prefer_billers_p)
        if channel_idx == 3:
            return self._emit_external_unknown(event_frame)
        return self._emit_merchant(event_frame)

    def _emit_p2p(self, event_frame: _EventFrame) -> Txn | None:
        ctx = self.request.ctx

        person_idx = event_frame.person_idx
        contact_idx = as_int(
            cast(
                int | np.integer,
                ctx.social.contacts[
                    person_idx,
                    self.request.rng.int(0, ctx.social.k_contacts),
                ],
            )
        )
        if not (0 <= contact_idx < len(ctx.persons)):
            return None

        dst_person = ctx.persons[contact_idx]
        dst_acct = ctx.primary_acct_for_person.get(dst_person)
        if dst_acct is None or dst_acct == event_frame.person_state.deposit_acct:
            return None

        amount = float(p2p_amount(self.request.rng)) * float(
            event_frame.person_state.persona.amount_mult
        )
        amount = round(max(1.0, amount), 2)

        return event_frame.txf.make(
            TxnSpec(
                src=event_frame.person_state.deposit_acct,
                dst=dst_acct,
                amt=amount,
                ts=event_frame.txn_ts,
                channel="p2p",
            )
        )

    def _emit_bill(
        self,
        event_frame: _EventFrame,
        prefer_billers_p: float,
    ) -> Txn:
        ctx = self.request.ctx
        person_idx = event_frame.person_idx
        person_state = event_frame.person_state

        if person_state.biller_k > 0 and self.request.rng.coin(prefer_billers_p):
            biller_idx = as_int(
                cast(
                    int | np.integer,
                    ctx.billers_idx[
                        person_idx,
                        self.request.rng.int(0, max(1, person_state.biller_k)),
                    ],
                )
            )
        else:
            biller_idx = cdf_pick(ctx.global_biller_cdf, self.request.rng.float())

        dst_acct = ctx.merchants.counterparty_acct[biller_idx]
        amount = float(bill_amount(self.request.rng))

        return event_frame.txf.make(
            TxnSpec(
                src=person_state.deposit_acct,
                dst=dst_acct,
                amt=amount,
                ts=event_frame.txn_ts,
                channel="bill",
            )
        )

    def _emit_external_unknown(self, event_frame: _EventFrame) -> Txn:
        merchants = self.request.ctx.merchants

        if merchants.external_accounts:
            dst_acct = self.request.rng.choice(merchants.external_accounts)
        else:
            dst_acct = "X0000000001"

        amount = float(p2p_amount(self.request.rng))
        amount = round(max(1.0, amount), 2)

        return event_frame.txf.make(
            TxnSpec(
                src=event_frame.person_state.deposit_acct,
                dst=dst_acct,
                amt=amount,
                ts=event_frame.txn_ts,
                channel="external_unknown",
            )
        )

    def _emit_merchant(self, event_frame: _EventFrame) -> Txn:
        dst_acct = self._merchant_destination(event_frame)
        src_acct, channel = self._merchant_source(event_frame)

        amount = float(p2p_amount(self.request.rng)) * float(
            event_frame.person_state.persona.amount_mult
        )
        amount = round(max(1.0, amount), 2)

        return event_frame.txf.make(
            TxnSpec(
                src=src_acct,
                dst=dst_acct,
                amt=amount,
                ts=event_frame.txn_ts,
                channel=channel,
            )
        )

    def _merchant_destination(self, event_frame: _EventFrame) -> str:
        ctx = self.request.ctx
        person_idx = event_frame.person_idx
        person_state = event_frame.person_state

        exploring = self.request.rng.coin(event_frame.explore_p)

        if (not exploring) and person_state.favorite_k > 0:
            merchant_idx = as_int(
                cast(
                    int | np.integer,
                    ctx.fav_merchants_idx[
                        person_idx,
                        self.request.rng.int(0, person_state.favorite_k),
                    ],
                )
            )
        else:
            merchant_idx = cdf_pick(ctx.merch_cdf, self.request.rng.float())
            favorite_row: ArrI32 = np.asarray(
                ctx.fav_merchants_idx[person_idx, :],
                dtype=np.int32,
            )

            tries = 0
            retry_limit = int(self.request.policy.merchant_retry_limit)
            while (
                person_state.favorite_k > 0
                and _row_contains(favorite_row, person_state.favorite_k, merchant_idx)
                and tries < retry_limit
            ):
                merchant_idx = cdf_pick(ctx.merch_cdf, self.request.rng.float())
                tries += 1

        return ctx.merchants.counterparty_acct[merchant_idx]

    def _merchant_source(self, event_frame: _EventFrame) -> tuple[str, str]:
        src_acct = event_frame.person_state.deposit_acct
        channel = "merchant"

        credit_runtime = _CreditRuntime(
            policy=self.request.credit_policy,
            card_for_person=self.request.card_for_person,
        )
        if not credit_runtime.enabled():
            return src_acct, channel

        policy = self.request.credit_policy
        card_map = self.request.card_for_person
        if policy is None or card_map is None:
            return src_acct, channel

        card_acct = card_map.get(event_frame.person_state.person_id)
        if card_acct is None:
            return src_acct, channel

        credit_share = float(
            policy.merchant_credit_share(event_frame.person_state.persona_name)
        )
        if self.request.rng.coin(credit_share):
            return card_acct, "card_purchase"

        return src_acct, channel


def generate_day_to_day_superposition(
    request: DayToDayGenerationRequest,
) -> list[Txn]:
    return _DayToDayGenerator(request=request).generate()
