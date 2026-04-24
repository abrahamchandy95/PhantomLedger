from bisect import bisect_right
from collections import Counter, defaultdict
from collections.abc import Iterable
from dataclasses import dataclass, field, replace
from datetime import date, datetime, timedelta
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
    LOC_INTEREST,
    MERCHANT,
    MORTGAGE_PAYMENT,
    OVERDRAFT_FEE,
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
from common.externals import BANK_FEE_COLLECTION, BANK_OD_LOC
from common.ids import is_external
from common.random import Rng
from common.transactions import Transaction
from transfers.balances import (
    ClearingHouse,
    REJECT_INSUFFICIENT_FUNDS,
    TransferDecision,
)


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

    # Overdraft-fee policy
    #
    # Bankrate 2025 industry average: $26.95 per item. Wells Fargo and
    # peer banks cap fees at 3 per day on most checking products; we
    # use that cap as a reasonable ceiling on daily fee volume.
    overdraft_fee_amount: float = 27.0
    overdraft_fee_daily_cap: int = 3

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


# ---------------------------------------------------------------------------
# Queue items
# ---------------------------------------------------------------------------


_KIND_TXN = "txn"
_KIND_LOC_BILLING = "loc_billing"


@dataclass(order=True, frozen=True, slots=True)
class _QueuedItem:
    """
    Heap entry for the replay's priority queue.

    Ordering is purely (timestamp, sequence). Sequence is a monotonically
    increasing tiebreaker assigned on push, so items at the same timestamp
    process in FIFO insertion order — transactions first, billing events
    last (so a billing event on the same instant as a user transaction
    picks up the transaction's balance change before computing interest).
    """

    timestamp: datetime
    sequence: int
    kind: str = field(default=_KIND_TXN, compare=False)
    retry_count: int = field(default=0, compare=False)
    txn: Transaction | None = field(default=None, compare=False)
    loc_account_idx: int | None = field(default=None, compare=False)
    loc_account_id: str | None = field(default=None, compare=False)


# ---------------------------------------------------------------------------
# Accumulator
# ---------------------------------------------------------------------------


@dataclass(slots=True)
class ChronoReplayAccumulator:
    """
    Chronological replay utility with cure / retry plus liquidity events.

    Balance-gated replay of a pre-sorted transaction stream. For each
    item the accumulator attempts the transfer; on an insufficient-funds
    rejection it may:
      - defer the item until a known future inbound transaction "cures"
        the shortfall, or
      - blind-retry a while later with bounded probability.

    When `emit_liquidity_events` is True, the accumulator also:
      - emits $27 overdraft-fee transactions (capped at 3/day/account)
        after any accepted debit that tapped a COURTESY-protected account
      - pre-generates monthly LOC billing events on each LOC account's
        cycle day, interleaved into the heap so interest postings appear
        in the output stream at the correct timestamp

    Disabling the flag is how the post-fraud replay avoids double-emitting
    liquidity events that already appeared in its input (having been
    produced by the authoritative pre-fraud replay).
    """

    book: ClearingHouse | None
    rng: Rng | None = None
    policy: ReplayPolicy = field(default_factory=ReplayPolicy)
    emit_liquidity_events: bool = True

    txns: list[Transaction] = field(default_factory=list)
    drop_counts: Counter[str] = field(default_factory=Counter)
    drop_counts_by_channel: Counter[tuple[str, str]] = field(default_factory=Counter)

    _next_sequence: int = field(default=0, init=False)
    _future_inbound_times: dict[str, list[datetime]] = field(
        default_factory=dict, init=False
    )
    _fee_taps_today: dict[tuple[int, date], int] = field(
        default_factory=dict, init=False
    )

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

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
            if self.emit_liquidity_events and decision.courtesy_tapped_idx is not None:
                self._maybe_emit_overdraft_fee(txn, decision)
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

        # Build reverse account-index map once for LOC billing emissions.
        idx_to_account = {idx: acct for acct, idx in self.book.account_indices.items()}

        # Anchor LOC clocks to the first input timestamp so the initial
        # billing cycle captures any negative balance from day one.
        if ordered and self.book.loc_accounts:
            first_ts = ordered[0].timestamp
            for acct_idx in self.book.loc_accounts:
                if acct_idx not in self.book.loc_last_update:
                    self.book.loc_last_update[acct_idx] = first_ts

        # Pre-generate monthly LOC billing events only when emission is on.
        billing_events: list[tuple[datetime, int, str]] = []
        if self.emit_liquidity_events and ordered and self.book.loc_accounts:
            window_start = ordered[0].timestamp
            # Extend a month past the last txn so the final period gets
            # billed if the simulation ends mid-cycle.
            window_end = ordered[-1].timestamp + timedelta(days=31)
            billing_events = self._generate_loc_billing_dates(
                window_start,
                window_end,
                idx_to_account,
            )

        pending: list[_QueuedItem] = []

        # Push transactions first so same-timestamp ties settle balance
        # changes before billing events measure them.
        for txn in ordered:
            heapq.heappush(
                pending,
                _QueuedItem(
                    timestamp=txn.timestamp,
                    sequence=self._consume_sequence(),
                    kind=_KIND_TXN,
                    retry_count=0,
                    txn=txn,
                ),
            )

        for billing_ts, acct_idx, account_id in billing_events:
            heapq.heappush(
                pending,
                _QueuedItem(
                    timestamp=billing_ts,
                    sequence=self._consume_sequence(),
                    kind=_KIND_LOC_BILLING,
                    loc_account_idx=acct_idx,
                    loc_account_id=account_id,
                ),
            )

        while pending:
            item = heapq.heappop(pending)

            if item.kind == _KIND_LOC_BILLING:
                self._process_loc_billing(item)
                continue

            # item.kind == _KIND_TXN
            txn = item.txn
            if txn is None:
                continue

            decision = self.book.try_transfer_with_reason(
                txn.source,
                txn.target,
                float(txn.amount),
                channel=txn.channel,
                timestamp=txn.timestamp,
            )
            if decision.accepted:
                self.txns.append(txn)
                if (
                    self.emit_liquidity_events
                    and decision.courtesy_tapped_idx is not None
                ):
                    self._maybe_emit_overdraft_fee(txn, decision)
                continue

            reason = decision.reason or REJECT_INSUFFICIENT_FUNDS
            if reason != REJECT_INSUFFICIENT_FUNDS:
                self._record_drop(reason, txn.channel)
                continue

            retry_ts = self._resolve_retry_timestamp(txn, item.retry_count)
            if retry_ts is None:
                self._record_drop(self._terminal_reason(txn.channel), txn.channel)
                continue

            if item.retry_count + 1 > self.policy.max_retries_for(txn.channel):
                self._record_drop("insufficient_funds_retry_exhausted", txn.channel)
                continue

            heapq.heappush(
                pending,
                _QueuedItem(
                    timestamp=retry_ts,
                    sequence=self._consume_sequence(),
                    kind=_KIND_TXN,
                    retry_count=item.retry_count + 1,
                    txn=replace(txn, timestamp=retry_ts),
                ),
            )

    # ------------------------------------------------------------------
    # Drop bookkeeping
    # ------------------------------------------------------------------

    def _record_drop(self, reason: str, channel: str | None) -> None:
        ch = channel or "none"
        self.drop_counts[reason] += 1
        self.drop_counts_by_channel[(ch, reason)] += 1

    def _consume_sequence(self) -> int:
        seq = self._next_sequence
        self._next_sequence += 1
        return seq

    # ------------------------------------------------------------------
    # Cure / retry
    # ------------------------------------------------------------------

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

    # ------------------------------------------------------------------
    # Overdraft fees
    # ------------------------------------------------------------------

    def _maybe_emit_overdraft_fee(
        self,
        txn: Transaction,
        decision: TransferDecision,
    ) -> None:
        if decision.courtesy_tapped_idx is None or self.book is None:
            return

        idx = decision.courtesy_tapped_idx

        # Per-account fee, driven by bank-tier assignment at init time.
        # ZERO_FEE-tier accounts carry fee=0.0 and emit no fee transaction,
        # even when they cross into courtesy coverage.
        fee_amount = float(self.book.overdraft_fee_amount.item(idx))
        if fee_amount <= 0.0:
            return

        day = txn.timestamp.date()
        key = (idx, day)
        count = self._fee_taps_today.get(key, 0)
        if count >= self.policy.overdraft_fee_daily_cap:
            return

        fee_ts = txn.timestamp + timedelta(minutes=1)

        # Fees bypass the insufficient-funds check inside balances.py via
        # the LIQUIDITY_EVENT_CHANNELS carve-out. They also suppress
        # further courtesy-tap detection, so this call cannot recurse.
        fee_decision = self.book.try_transfer_with_reason(
            txn.source,
            BANK_FEE_COLLECTION,
            fee_amount,
            channel=OVERDRAFT_FEE,
            timestamp=fee_ts,
        )
        if not fee_decision.accepted:
            return

        self.txns.append(
            Transaction(
                source=txn.source,
                target=BANK_FEE_COLLECTION,
                amount=fee_amount,
                timestamp=fee_ts,
                fraud_flag=0,
                fraud_ring_id=-1,
                device_id=txn.device_id,
                ip_address=txn.ip_address,
                channel=OVERDRAFT_FEE,
            )
        )
        self._fee_taps_today[key] = count + 1

    # ------------------------------------------------------------------
    # LOC billing
    # ------------------------------------------------------------------

    def _generate_loc_billing_dates(
        self,
        start_ts: datetime,
        end_ts: datetime,
        idx_to_account: dict[int, str],
    ) -> list[tuple[datetime, int, str]]:
        """
        Monthly billing timestamp per LOC account.

        Each account has a fixed billing_day in [1, 28] (no month-length
        special cases). We iterate first-of-month from the start of the
        replay window through `end_ts`, emitting one (timestamp, idx,
        account_id) tuple per billing that falls strictly within the
        window. Billing fires at 23:55 on the cycle day so most of the
        day's transactions have already posted by the time interest is
        computed.
        """
        if self.book is None or not self.book.loc_accounts:
            return []

        out: list[tuple[datetime, int, str]] = []

        for acct_idx in self.book.loc_accounts:
            billing_day = self.book.loc_billing_day.get(acct_idx, 15)
            account_id = idx_to_account.get(acct_idx)
            if account_id is None:
                continue

            cursor = datetime(start_ts.year, start_ts.month, 1)

            while cursor < end_ts:
                billing_ts = cursor.replace(
                    day=billing_day,
                    hour=23,
                    minute=55,
                    second=0,
                    microsecond=0,
                )
                if start_ts <= billing_ts < end_ts:
                    out.append((billing_ts, acct_idx, account_id))

                # Advance one calendar month.
                if cursor.month == 12:
                    cursor = cursor.replace(year=cursor.year + 1, month=1)
                else:
                    cursor = cursor.replace(month=cursor.month + 1)

        return out

    def _process_loc_billing(self, item: _QueuedItem) -> None:
        if (
            self.book is None
            or item.loc_account_idx is None
            or item.loc_account_id is None
        ):
            return

        interest = self.book.bill_loc_interest(item.loc_account_idx, item.timestamp)
        if interest <= 0.0:
            return

        # Interest posting is a record-only transaction: bill_loc_interest
        # has already decremented the account's balance directly. We don't
        # call try_transfer here (that would double-debit). The post-fraud
        # replay processes this Transaction through try_transfer normally
        # because its emit_liquidity_events flag is False, so billing
        # logic doesn't run and the balance-level effect matches this one.
        self.txns.append(
            Transaction(
                source=item.loc_account_id,
                target=BANK_OD_LOC,
                amount=interest,
                timestamp=item.timestamp,
                fraud_flag=0,
                fraud_ring_id=-1,
                device_id=None,
                ip_address=None,
                channel=LOC_INTEREST,
            )
        )


# Backward-compat alias. Safe to delete later once all imports are updated.
TxnAccumulator = ChronoReplayAccumulator
