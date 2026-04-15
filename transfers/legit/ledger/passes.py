from transfers.family.engine import GraphConfig, TransferConfig
from transfers.factory import TransactionFactory
from transfers.government import generate as generate_government_txns
from transfers.legit.blueprints import Blueprint, LegitBuildPlan
from transfers.legit.inflows import (
    generate_rent_txns,
    generate_salary_txns,
)
from transfers.legit.inflows.revenue import build_revenue
from transfers.legit.routines import (
    generate_atm_txns,
    generate_credit_lifecycle_txns,
    generate_day_to_day_txns,
    generate_family_txns,
    generate_internal_txns,
    generate_subscription_txns,
    split_deposits,
)

from .screenbook import ScreenBook
from .streams import TxnStreams


def add_income(
    request: Blueprint,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
    streams: TxnStreams,
) -> None:
    timeline = request.timeline

    streams.add(generate_salary_txns(timeline, request.specs, plan, txf))

    streams.add(
        generate_government_txns(
            request.macro.government,
            timeline.window,
            timeline.rng,
            txf,
            personas=plan.personas.persona_for_person,
            primary_accounts=plan.primary_acct_for_person,
        )
    )

    streams.add(build_revenue(request, plan, txf))


def add_routines(
    request: Blueprint,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
    streams: TxnStreams,
    screen: ScreenBook,
) -> None:
    timeline = request.timeline
    accounts_by_person = request.network.accounts.by_person

    streams.add(
        split_deposits(
            timeline.rng,
            plan,
            txf,
            accounts_by_person,
            streams.candidates,
        )
    )

    streams.add(
        generate_rent_txns(
            timeline,
            request.network,
            request.specs,
            plan,
            txf,
        )
    )

    streams.add(
        generate_subscription_txns(
            timeline.rng,
            plan,
            txf,
            book=screen.fresh(),
            base_txns=streams.screened,
            base_txns_sorted=True,
        )
    )

    streams.add(
        generate_atm_txns(
            timeline.rng,
            plan,
            txf,
            book=screen.fresh(),
            base_txns=streams.screened,
            base_txns_sorted=True,
        )
    )

    streams.add(
        generate_internal_txns(
            timeline.rng,
            plan,
            txf,
            accounts_by_person,
            book=screen.fresh(),
            base_txns=streams.screened,
            base_txns_sorted=True,
        )
    )

    streams.add(
        generate_day_to_day_txns(
            request,
            plan,
            streams.screened,
            screen_book=screen.fresh(),
            base_txns_sorted=True,
        )
    )


def add_family(
    request: Blueprint,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
    streams: TxnStreams,
    fam_graph_cfg: GraphConfig,
    fam_transfer_cfg: TransferConfig,
) -> None:
    streams.add(
        generate_family_txns(
            request,
            fam_graph_cfg,
            fam_transfer_cfg,
            plan,
            txf,
        )
    )


def add_credit(
    request: Blueprint,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
    streams: TxnStreams,
) -> None:
    streams.add(
        generate_credit_lifecycle_txns(
            request,
            plan,
            txf,
            streams.candidates,
        )
    )
