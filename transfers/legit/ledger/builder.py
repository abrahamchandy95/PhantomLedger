from dataclasses import dataclass

from transfers.family.engine import GraphConfig, TransferConfig
from transfers.factory import TransactionFactory

from .limits import build_balance_book
from .passes import add_credit, add_family, add_income, add_routines
from .screenbook import ScreenBook
from .streams import TxnStreams

from transfers.legit.blueprints import (
    Blueprint,
    TransfersPayload,
    build_legit_plan,
)


@dataclass(slots=True)
class LegitTransferBuilder:
    request: Blueprint
    fam_graph_cfg: GraphConfig
    fam_transfer_cfg: TransferConfig

    def payload(self) -> TransfersPayload:
        if not self.request.network.accounts.ids:
            return TransfersPayload(
                candidate_txns=[],
                hub_accounts=[],
                biller_accounts=[],
                employers=[],
                initial_book=None,
            )

        plan = build_legit_plan(
            self.request.timeline,
            self.request.network,
            self.request.macro,
            self.request.overrides,
        )

        initial_book = build_balance_book(
            self.request.timeline,
            self.request.network,
            self.request.specs,
            self.request.cc_state,
            plan,
        )

        txf = TransactionFactory(
            rng=self.request.timeline.rng,
            infra=self.request.overrides.infra,
        )

        streams = TxnStreams()
        screen = ScreenBook(initial_book)

        add_income(self.request, plan, txf, streams)
        add_routines(self.request, plan, txf, streams, screen)
        add_family(
            self.request,
            plan,
            txf,
            streams,
            self.fam_graph_cfg,
            self.fam_transfer_cfg,
        )
        add_credit(self.request, plan, txf, streams)

        return TransfersPayload(
            candidate_txns=streams.candidates,
            hub_accounts=plan.counterparties.hub_accounts,
            biller_accounts=plan.counterparties.biller_accounts,
            employers=plan.counterparties.employers,
            initial_book=initial_book,
            replay_sorted_txns=streams.replay_ready,
        )

    def build(self) -> TransfersPayload:
        return self.payload()
