from dataclasses import dataclass
from datetime import timedelta
from typing import cast

import numpy as np

from common.math import ArrI32, as_int, cdf_pick
from common.transactions import Transaction
from math_models.timing import sample_offsets_seconds
from transfers.txns import TxnFactory

from .channels import build_channel_cdf, build_channel_txn
from .models import DayToDayGenerationRequest
from .state import (
    EventFrame,
    build_day_frame,
    build_person_state,
    compute_explore_probability,
    compute_outgoing_count,
)


@dataclass(slots=True)
class DayToDayGenerator:
    request: DayToDayGenerationRequest

    def generate(self) -> list[Transaction]:
        if self.request.days <= 0:
            return []

        self.request.policy.validate()

        txf = TxnFactory(rng=self.request.rng, infra=self.request.infra)
        channel_cdf = build_channel_cdf(self.request)

        base_per_day = (
            float(self.request.merchants_cfg.target_payments_per_person_per_month)
            / 30.0
        )
        prefer_billers_p = float(self.request.events.prefer_billers_p)
        day_cap = int(self.request.events.max_events_per_day)

        txns: list[Transaction] = []
        n_people = len(self.request.ctx.persons)

        for day_index in range(self.request.days):
            day_frame = build_day_frame(self.request, day_index)
            produced_today = 0

            for person_idx in range(n_people):
                if day_cap > 0 and produced_today >= day_cap:
                    break

                person_state = build_person_state(self.request, person_idx)
                if person_state is None:
                    continue

                remaining_today = (
                    None if day_cap <= 0 else max(0, day_cap - produced_today)
                )
                n_out = compute_outgoing_count(
                    self.request,
                    person_state,
                    day_frame,
                    base_per_day,
                    remaining_today,
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
                explore_p = compute_explore_probability(
                    self.request,
                    person_state,
                    day_frame,
                )

                for event_idx in range(n_out):
                    offset_seconds = as_int(cast(int | np.integer, offsets[event_idx]))
                    event_frame = EventFrame(
                        person_idx=person_idx,
                        person_state=person_state,
                        txn_ts=day_frame.day_start + timedelta(seconds=offset_seconds),
                        txf=txf,
                        explore_p=explore_p,
                    )

                    channel_idx = cdf_pick(channel_cdf, self.request.rng.float())
                    txn = build_channel_txn(
                        channel_idx,
                        self.request,
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


def generate_day_to_day_superposition(
    request: DayToDayGenerationRequest,
) -> list[Transaction]:
    return DayToDayGenerator(request=request).generate()
