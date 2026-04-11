from collections import Counter
from pathlib import Path

from common.channels import FRAUD_CHANNELS
from pipeline.state import Entities, Transfers


def print_summary(
    out_dir: Path,
    entities: Entities,
    transfers: Transfers,
    unique_has_paid_edges: int,
) -> None:
    total_before = len(transfers.draft_txns)
    total_after = len(transfers.final_txns)
    illicit = sum(1 for txn in transfers.final_txns if txn.fraud_flag == 1)
    ratio = illicit / max(1, total_after)

    print(f"Wrote outputs to: {out_dir}")
    print(
        f"People: {len(entities.people.ids)}  "
        + f"Accounts: {len(entities.accounts.ids)}"
    )
    print(f"Transactions: {total_after} (before fraud: {total_before})")
    print(
        f"Illicit txns: {illicit} ({ratio:.6%})  "
        + f"Injected: {transfers.fraud.injected_count}"
    )
    print(f"Unique HAS_PAID edges: {unique_has_paid_edges}")

    if transfers.drop_counts:
        dropped_total = sum(transfers.drop_counts.values())
        top_drops = Counter(transfers.drop_counts).most_common(5)
        breakdown = ", ".join(f"{reason}={count}" for reason, count in top_drops)
        print(f"Dropped pre-fraud txns: {dropped_total} ({breakdown})")

    if transfers.drop_counts_by_channel:
        top_channel_drops = Counter(transfers.drop_counts_by_channel).most_common(12)
        breakdown = ", ".join(
            f"{channel}[{reason}]={count}"
            for (channel, reason), count in top_channel_drops
        )
        print(f"Dropped pre-fraud by channel×reason: {breakdown}")

    # Channel breakdown — validates that fraud channels align with flags
    channel_counts: Counter[str] = Counter()
    fraud_by_channel: Counter[str] = Counter()

    for txn in transfers.final_txns:
        ch = txn.channel or "none"
        channel_counts[ch] += 1
        if ch in FRAUD_CHANNELS:
            fraud_by_channel[ch] += 1

    flag_mismatches = sum(
        1
        for txn in transfers.final_txns
        if (txn.channel or "") in FRAUD_CHANNELS and txn.fraud_flag != 1
    )

    print(f"Channels: {len(channel_counts)} distinct")
    if fraud_by_channel:
        top = fraud_by_channel.most_common(5)
        breakdown = ", ".join(f"{ch}={n}" for ch, n in top)
        print(f"Fraud channels: {breakdown}")
    if flag_mismatches > 0:
        print(f"WARNING: {flag_mismatches} txns have fraud channel but flag=0")
