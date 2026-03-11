from common.transactions import Transaction
from pipeline.state import GenerationState, TransferStageData

from .requests import build_fraud_request, build_legit_request
from .sorting import txn_sort_key
import transfers.fraud as fraud_model
import transfers.legit as legit_model


def build_transfers(st: GenerationState) -> None:
    _ = st.require_entities()

    legit_request = build_legit_request(st)
    legit_result: legit_model.LegitTransfers = legit_model.LegitTransferBuilder(
        request=legit_request
    ).build()

    base_txns: list[Transaction] = legit_result.txns
    biller_accounts: list[str] = legit_result.biller_accounts
    employers: list[str] = legit_result.employers

    fraud_request = build_fraud_request(
        st,
        base_txns=base_txns,
        biller_accounts=biller_accounts,
        employers=employers,
    )
    fraud_result: fraud_model.FraudInjectionResult = fraud_model.inject_fraud_transfers(
        fraud_request
    )

    st.transfers = TransferStageData(
        legit=legit_result,
        fraud=fraud_result,
        base_txns=base_txns,
        all_txns=sorted(fraud_result.txns, key=txn_sort_key),
    )
