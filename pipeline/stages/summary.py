from pipeline.state import GenerationState


def print_summary(st: GenerationState) -> None:
    entities = st.require_entities()
    transfers = st.require_transfers()

    total_before = len(transfers.base_txns)
    total_after = len(transfers.all_txns)
    illicit = sum(1 for txn in transfers.all_txns if txn.fraud_flag == 1)
    ratio = illicit / max(1, total_after)

    print(f"Wrote outputs to: {st.out_dir}")
    print(
        f"People: {len(entities.people.people)}  Accounts: {len(entities.accounts.accounts)}"
    )
    print(f"Transactions: {total_after} (before fraud: {total_before})")
    print(
        f"Illicit txns: {illicit} ({ratio:.6%})  Injected: {transfers.fraud.injected_count}"
    )
    print(f"Unique HAS_PAID edges: {st.unique_has_paid_edges}")
