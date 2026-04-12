from bisect import bisect_right
from collections import Counter, defaultdict
from collections.abc import Iterable
from dataclasses import dataclass, field, replace
from datetime import datetime, timedelta
import heapq

from common.channels import (
    ALLOWANCE,
    ATM,
    AUTO_LOAN_PAYMENT,
    BILL,
    CARD_PURCHASE,
    CC_REFUND,
    DISABILITY,
    EXTERNAL_UNKNOWN,
    FAMILY_SUPPORT,
    GOV_PENSION,
    GOV_SOCIAL_SECURITY,
    GRANDPARENT_GIFT,
    INHERITANCE,
    INSURANCE_CLAIM,
    INSURANCE_PREMIUM,
    MERCHANT,
    MORTGAGE_PAYMENT,
    P2P,
    PARENT_GIFT,
    RENT,
    SALARY,
    SELF_TRANSFER,
    SIBLING_TRANSFER,
    SPOUSE_TRANSFER,
    STUDENT_LOAN_PAYMENT,
    SUBSCRIPTION,
    TAX_ESTIMATED_PAYMENT,
    TAX_REFUND,
    TUITION,
    CARD_SETTLEMENT,
    CLIENT_ACH_CREDIT,
    INVESTMENT_INFLOW,
    OWNER_DRAW,
    PLATFORM_PAYOUT,
)
from common.ids import is_external
from common.random import Rng
from common.transactions import Transaction
from transfers.balances import Ledger, REJECT_INSUFFICIENT_FUNDS


CARD_LIKE_CHANNELS: frozenset[str] = frozenset(
    {
        ATM,
        MERCHANT,
        CARD_PURCHASE,
        P2P,
    }
)

RETRYABLE_CHANNELS: frozenset[str] = frozenset(
    {
        BILL,
        RENT,
        SUBSCRIPTION,
        EXTERNAL_UNKNOWN,
        INSURANCE_PREMIUM,
        MORTGAGE_PAYMENT,
        AUTO_LOAN_PAYMENT,
        STUDENT_LOAN_PAYMENT,
        TAX_ESTIMATED_PAYMENT,
    }
)

CURE_INBOUND_CHANNELS: frozenset[str] = frozenset(
    {
        SALARY,
        GOV_SOCIAL_SECURITY,
        GOV_PENSION,
        DISABILITY,
        INSURANCE_CLAIM,
        TAX_REFUND,
        SELF_TRANSFER,
        ALLOWANCE,
        TUITION,
        FAMILY_SUPPORT,
        SPOUSE_TRANSFER,
        PARENT_GIFT,
        SIBLING_TRANSFER,
        GRANDPARENT_GIFT,
        INHERITANCE,
        CC_REFUND,
        CLIENT_ACH_CREDIT,
        CARD_SETTLEMENT,
        PLATFORM_PAYOUT,
        OWNER_DRAW,
        INVESTMENT_INFLOW,
    }
)


@dataclass(frozen=True, slots=True)
class ReplayPolicy:
    same_day_cure_hours: int = 10
    delayed_cure_hours: int = 36
    card_retry_padding_minutes: int = 5
    ach_retry_padding_minutes: int = 30
    blind_retry_delay_hours: int = 18
    blind_retry_second_delay_hours: int = 72
    blind_retry_probability: float = 0.55

    def max_retries_for(self, channel: str | None) -> int:
        if channel in CARD_LIKE_CHANNELS:
            return 1
        if channel in RETRYABLE_CHANNELS:
            return 2
        return 1


DEFAULT_REPLAY_POLICY = ReplayPolicy()

type ReplaySortKey = tuple[datetime, str, str, float]


def replay_sort_key(txn: Transaction) -> ReplaySortKey:
    """
    Deterministic ordering used by the authoritative pre-fraud replay.

    Important:
    This intentionally matches the existing ChronoReplayAccumulator.extend()
    ordering exactly. Do not widen this key unless you explicitly want to
    change replay semantics.
    """
    return (
        txn.timestamp,
        txn.source,
        txn.target,
        float(txn.amount),
    )


def sort_for_replay(items: Iterable[Transaction]) -> list[Transaction]:
    return sorted(items, key=replay_sort_key)


def merge_replay_sorted(
    existing: list[Transaction],
    new_items: Iterable[Transaction],
) -> list[Transaction]:
    """
    Merge a new stage stream into an already replay-sorted prefix.

    Stability matters:
    - ties stay in `existing` before `new_items`
    - that matches the current behavior of:
          sorted(existing + new_items, key=replay_sort_key)
      because Python's sort is stable and later stages are appended after
      earlier stages.
    """
    new_sorted = sort_for_replay(new_items)
    if not new_sorted:
        return existing
    if not existing:
        return new_sorted

    out: list[Transaction] = []
    left = 0
    right = 0

    while left < len(existing) and right < len(new_sorted):
        if replay_sort_key(existing[left]) <= replay_sort_key(new_sorted[right]):
            out.append(existing[left])
            left += 1
        else:
            out.append(new_sorted[right])
            right += 1

    if left < len(existing):
        out.extend(existing[left:])
    if right < len(new_sorted):
        out.extend(new_sorted[right:])

    return out


@dataclass(order=True, frozen=True, slots=True)
class _QueuedTxn:
    timestamp: datetime
    sequence: int
    retry_count: int
    txn: Transaction = field(compare=False)


@dataclass(slots=True)
class ChronoReplayAccumulator:
    """
    Chronological replay utility with a small cure / retry layer.

    This remains the single authoritative balance gate, but instead of turning
    every low-cash debit into a permanent failure immediately, it can defer some
    items until later incoming funds or one retry attempt.
    """

    book: Ledger | None
    rng: Rng | None = None
    policy: ReplayPolicy = field(default_factory=ReplayPolicy)
    txns: list[Transaction] = field(default_factory=list)
    drop_counts: Counter[str] = field(default_factory=Counter)
    drop_counts_by_channel: Counter[tuple[str, str]] = field(default_factory=Counter)
    _next_sequence: int = field(default=0, init=False)
    _future_inbound_times: dict[str, list[datetime]] = field(
        default_factory=dict, init=False
    )

    def _record_drop(self, reason: str, channel: str | None) -> None:
        ch = channel or "none"
        self.drop_counts[reason] += 1
        self.drop_counts_by_channel[(ch, reason)] += 1

    def append(self, txn: Transaction) -> bool:
        if self.book is None:
            self.txns.append(txn)
            return True

        decision = self.book.try_transfer_with_reason(
            txn.source,
            txn.target,
            float(txn.amount),
            channel=txn.channel,
            timestamp=txn.timestamp,
        )
        if decision.accepted:
            self.txns.append(txn)
            return True

        if decision.reason is not None:
            self._record_drop(decision.reason, txn.channel)
        return False

    def extend(
        self,
        items: Iterable[Transaction],
        *,
        presorted: bool = False,
    ) -> None:
        ordered = list(items) if presorted else sort_for_replay(items)

        if self.book is None:
            self.txns.extend(ordered)
            return

        self._future_inbound_times = self._build_future_inbound_index(ordered)
        pending: list[_QueuedTxn] = []

        for txn in ordered:
            heapq.heappush(
                pending,
                _QueuedTxn(
                    timestamp=txn.timestamp,
                    sequence=self._consume_sequence(),
                    retry_count=0,
                    txn=txn,
                ),
            )

        while pending:
            queued = heapq.heappop(pending)
            txn = queued.txn

            decision = self.book.try_transfer_with_reason(
                txn.source,
                txn.target,
                float(txn.amount),
                channel=txn.channel,
                timestamp=txn.timestamp,
            )
            if decision.accepted:
                self.txns.append(txn)
                continue

            reason = decision.reason or REJECT_INSUFFICIENT_FUNDS
            if reason != REJECT_INSUFFICIENT_FUNDS:
                self._record_drop(reason, txn.channel)
                continue

            retry_ts = self._resolve_retry_timestamp(txn, queued.retry_count)
            if retry_ts is None:
                self._record_drop(self._terminal_reason(txn.channel), txn.channel)
                continue

            if queued.retry_count + 1 > self.policy.max_retries_for(txn.channel):
                self._record_drop("insufficient_funds_retry_exhausted", txn.channel)
                continue

            heapq.heappush(
                pending,
                _QueuedTxn(
                    timestamp=retry_ts,
                    sequence=self._consume_sequence(),
                    retry_count=queued.retry_count + 1,
                    txn=replace(txn, timestamp=retry_ts),
                ),
            )

    def _consume_sequence(self) -> int:
        seq = self._next_sequence
        self._next_sequence += 1
        return seq

    def _build_future_inbound_index(
        self,
        items: list[Transaction],
    ) -> dict[str, list[datetime]]:
        by_account: dict[str, list[datetime]] = defaultdict(list)

        for txn in items:
            if not self._can_cure_with(txn):
                continue
            by_account[txn.target].append(txn.timestamp)

        return dict(by_account)

    def _can_cure_with(self, txn: Transaction) -> bool:
        if is_external(txn.source):
            return True
        return bool(txn.channel in CURE_INBOUND_CHANNELS)

    def _resolve_retry_timestamp(
        self,
        txn: Transaction,
        retry_count: int,
    ) -> datetime | None:
        cure_ts = self._find_future_cure(txn)
        if cure_ts is not None:
            padding_minutes = (
                self.policy.card_retry_padding_minutes
                if txn.channel in CARD_LIKE_CHANNELS
                else self.policy.ach_retry_padding_minutes
            )
            return cure_ts + timedelta(minutes=padding_minutes)

        if (
            self.rng is None
            or txn.channel not in RETRYABLE_CHANNELS
            or retry_count >= self.policy.max_retries_for(txn.channel)
        ):
            return None

        if not self.rng.coin(float(self.policy.blind_retry_probability)):
            return None

        delay_hours = (
            self.policy.blind_retry_delay_hours
            if retry_count == 0
            else self.policy.blind_retry_second_delay_hours
        )
        return txn.timestamp + timedelta(hours=delay_hours)

    def _find_future_cure(self, txn: Transaction) -> datetime | None:
        times = self._future_inbound_times.get(txn.source)
        if not times:
            return None

        cure_hours = (
            self.policy.same_day_cure_hours
            if txn.channel in CARD_LIKE_CHANNELS
            else self.policy.delayed_cure_hours
        )
        upper = txn.timestamp + timedelta(hours=cure_hours)

        start = bisect_right(times, txn.timestamp)
        for idx in range(start, len(times)):
            ts = times[idx]
            if ts > upper:
                break
            return ts

        return None

    def _terminal_reason(self, channel: str | None) -> str:
        if channel in CARD_LIKE_CHANNELS:
            return "insufficient_funds_instant_decline"
        if channel in RETRYABLE_CHANNELS:
            return "insufficient_funds_terminal"
        return REJECT_INSUFFICIENT_FUNDS


# Backward-compat alias. Safe to delete later once all imports are updated.
TxnAccumulator = ChronoReplayAccumulator
