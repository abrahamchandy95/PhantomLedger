from datetime import timedelta

from common import config
from common.progress import status
from common.random import Rng
from common.transactions import Transaction
from pipeline.invariants import validate_transaction_accounts
from pipeline.state import Entities, Infra, Transfers

from .requests import build_fraud, build_legit

from transfers.family.engine import GraphConfig, TransferConfig
from transfers.factory import TransactionFactory
from transfers.fraud import InjectionOutput, inject as inject_fraud
from transfers.insurance import generate as generate_insurance
from transfers.legit.blueprints import TransfersPayload
from transfers.legit.ledger import (
    ChronoReplayAccumulator,
    merge_replay_sorted,
    sort_for_replay,
)
from transfers.legit.ledger.builder import LegitTransferBuilder
from transfers.obligations import emit as emit_obligations


def build(
    cfg: config.World,
    rng: Rng,
    entities: Entities,
    infra: Infra,
) -> Transfers:
    status("Transfers: building legitimate transactions...")
    legit_request = build_legit(cfg, rng, entities, infra)

    fam_graph_cfg = GraphConfig(
        households=cfg.households,
        dependents=cfg.dependents,
        retiree_support=cfg.retiree_support,
    )

    fam_transfer_cfg = TransferConfig(
        allowances=cfg.allowances,
        tuition=cfg.tuition,
        retiree_support=cfg.retiree_support,
        spouses=cfg.spouses,
        parent_gifts=cfg.parent_gifts,
        sibling_transfers=cfg.sibling_transfers,
        grandparent_gifts=cfg.grandparent_gifts,
        inheritance=cfg.inheritance,
        routing=cfg.family_routing,
    )

    legit_result: TransfersPayload = LegitTransferBuilder(
        request=legit_request,
        fam_graph_cfg=fam_graph_cfg,
        fam_transfer_cfg=fam_transfer_cfg,
    ).build()

    # Preserve semantic generation order for downstream dependency-sensitive
    # consumers and generators.
    candidate_txns: list[Transaction] = list(legit_result.candidate_txns)
    biller_accounts: list[str] = legit_result.biller_accounts
    employers: list[str] = legit_result.employers

    # Replay-ready chronological order using the exact authoritative replay key:
    # (timestamp, source, target, amount).
    replay_candidate_txns: list[Transaction] = (
        list(legit_result.replay_sorted_txns)
        if legit_result.replay_sorted_txns
        else sort_for_replay(legit_result.candidate_txns)
    )

    primary_accounts = {
        pid: accts[0] for pid, accts in entities.accounts.by_person.items() if accts
    }

    gov_txf = TransactionFactory(rng=rng, infra=infra.router)

    # Insurance premiums and claims — reads from portfolio.
    status("Transfers: adding insurance and obligation flows...")
    ins_txns = generate_insurance(
        cfg.insurance,
        cfg.window,
        rng,
        gov_txf,
        base_seed=cfg.population.seed,
        portfolios=entities.portfolios,
        primary_accounts=primary_accounts,
    )
    candidate_txns.extend(ins_txns)
    replay_candidate_txns = merge_replay_sorted(replay_candidate_txns, ins_txns)

    # Financial product obligations (mortgages, loans, taxes).
    start = cfg.window.start_date
    end_excl = start + timedelta(days=int(cfg.window.days))

    obligation_txns = emit_obligations(
        entities.portfolios,
        start=start,
        end_excl=end_excl,
        primary_accounts=primary_accounts,
        rng=rng,
        txf=gov_txf,
    )
    candidate_txns.extend(obligation_txns)
    replay_candidate_txns = merge_replay_sorted(
        replay_candidate_txns,
        obligation_txns,
    )

    # Single authoritative pre-fraud balance pass from the pristine starting
    # ledger. This is the only place where balances decide keep/drop AND the
    # only place that emits liquidity events (overdraft fees, LOC interest).
    # Those emitted events become part of the draft ledger and are carried
    # through fraud injection and the post-fraud replay as ordinary txns.
    replay_acc = ChronoReplayAccumulator(
        book=None
        if legit_result.initial_book is None
        else legit_result.initial_book.copy(),
        rng=rng,
    )

    status("Transfers: replaying chronological balances...")
    replay_acc.extend(replay_candidate_txns, presorted=True)

    draft_txns = replay_acc.txns

    # Report only the authoritative chronological replay drops.
    drop_counts = dict(replay_acc.drop_counts)
    drop_counts_by_channel = dict(replay_acc.drop_counts_by_channel)

    # Inject the fraud logic into the drafts.
    fraud_request = build_fraud(
        cfg,
        rng,
        entities,
        infra,
        draft_txns=draft_txns,
        biller_accounts=biller_accounts,
        employers=employers,
    )

    status("Transfers: injecting fraud scenarios...")
    fraud_result: InjectionOutput = inject_fraud(fraud_request)

    # Post-fraud replay: re-runs the balance pass with fraud injected, but
    # must NOT emit liquidity events again — the pre-fraud replay already
    # produced authoritative overdraft fees and LOC interest postings, and
    # those are already present in fraud_result.txns as ordinary records.
    # Passing them through a second emitter would double-debit the ledger.
    status("Transfers: replaying post-fraud chronological balances...")
    final_replay = ChronoReplayAccumulator(
        book=None
        if legit_result.initial_book is None
        else legit_result.initial_book.copy(),
        rng=rng,
        emit_liquidity_events=False,
    )
    final_replay.extend(sort_for_replay(fraud_result.txns), presorted=True)

    final_txns = final_replay.txns
    validate_transaction_accounts(entities.accounts, final_txns)

    return Transfers(
        legit=legit_result,
        fraud=fraud_result,
        draft_txns=draft_txns,
        final_txns=final_txns,
        final_book=final_replay.book,
        drop_counts=drop_counts,
        drop_counts_by_channel=drop_counts_by_channel,
    )
