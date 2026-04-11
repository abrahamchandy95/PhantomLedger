from datetime import timedelta

from common import config
from common.random import Rng
from common.transactions import Transaction
from pipeline.invariants import validate_transaction_accounts
from pipeline.state import Entities, Infra, Transfers

from .requests import build_fraud, build_legit
from .sorting import key

from transfers.factory import TransactionFactory
from transfers.fraud import InjectionOutput, inject as inject_fraud
from transfers.insurance import generate as generate_insurance
from transfers.legit import LegitTransferBuilder, TransfersPayload
from transfers.legit.accumulator import ChronoReplayAccumulator
from transfers.obligations import emit as emit_obligations


def build(
    cfg: config.World,
    rng: Rng,
    entities: Entities,
    infra: Infra,
) -> Transfers:
    legit_request = build_legit(cfg, rng, entities, infra)
    legit_result: TransfersPayload = LegitTransferBuilder(request=legit_request).build()

    # Legit builder preserves semantic generation order for dependency purposes.
    # Balance enforcement happens once, here, over the full pre-fraud candidate
    # set in chronological order.
    candidate_txns: list[Transaction] = list(legit_result.candidate_txns)
    biller_accounts: list[str] = legit_result.biller_accounts
    employers: list[str] = legit_result.employers

    primary_accounts = {
        pid: accts[0] for pid, accts in entities.accounts.by_person.items() if accts
    }

    gov_txf = TransactionFactory(rng=rng, infra=infra.router)

    # Insurance premiums and claims — reads from portfolio.
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

    # Single authoritative pre-fraud balance pass from the pristine starting
    # ledger. This is the only place where balances decide keep/drop.
    replay_acc = ChronoReplayAccumulator(
        book=None
        if legit_result.initial_book is None
        else legit_result.initial_book.copy(),
        rng=rng,
    )
    replay_acc.extend(sorted(candidate_txns, key=key))

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
    fraud_result: InjectionOutput = inject_fraud(fraud_request)

    final_txns = sorted(fraud_result.txns, key=key)
    validate_transaction_accounts(entities.accounts, final_txns)

    return Transfers(
        legit=legit_result,
        fraud=fraud_result,
        draft_txns=draft_txns,
        final_txns=final_txns,
        drop_counts=drop_counts,
        drop_counts_by_channel=drop_counts_by_channel,
    )
