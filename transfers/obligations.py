"""
Obligation event emitter.

Converts ObligationEvents from the product portfolio into
concrete Transactions. This is the bridge between the pure
product layer (no infra, no fraud flags) and the transaction
pipeline (with device/IP routing and balance constraints).

Installment-loan behavior modeled here:
- occasional late payments
- missed monthly payments
- partial payments
- catch-up / cure payments
- delinquency clustering once an account falls behind

Why this lives here:
The product layer defines contractual due events.
This transfer layer decides how those due events appear in
observed transaction history.
"""

from dataclasses import dataclass
from datetime import datetime, timedelta

from common.random import Rng
from common.transactions import Transaction
from entities.products.auto_loan import AutoLoanTerms
from entities.products.event import Direction, ObligationEvent
from entities.products.mortgage import MortgageTerms
from entities.products.portfolio import PortfolioRegistry
from entities.products.student_loan import StudentLoanTerms
from transfers.factory import TransactionDraft, TransactionFactory

type InstallmentTerms = MortgageTerms | AutoLoanTerms | StudentLoanTerms


@dataclass(slots=True)
class _InstallmentState:
    """Running delinquency state for one person/product pair."""

    carry_due: float = 0.0
    delinquent_cycles: int = 0


_LOAN_PRODUCT_TYPES: frozenset[str] = frozenset(
    {"mortgage", "auto_loan", "student_loan"}
)


def _round_money(amount: float) -> float:
    return round(max(0.0, float(amount)), 2)


def _add_basic_jitter(rng: Rng, ts: datetime) -> datetime:
    """
    Add small timing jitter to scheduled non-loan events.

    Real payments do not land at exactly midnight on the due date.
    ACH processing, autopay timing, and manual habits introduce
    0-3 days and variable hours of offset.
    """
    return ts + timedelta(
        days=rng.int(0, 3),
        hours=rng.int(0, 12),
        minutes=rng.int(0, 60),
    )


def _uniform_between(rng: Rng, low: float, high: float) -> float:
    """
    Uniform float in [low, high].

    Uses Rng.float() because the wrapper does not expose a dedicated
    uniform() helper.
    """
    low_f = float(low)
    high_f = float(high)

    if high_f < low_f:
        raise ValueError("high must be >= low")

    if high_f == low_f:
        return low_f

    u = rng.float()
    return low_f + ((high_f - low_f) * u)


def _payment_timestamp(
    rng: Rng,
    due_ts: datetime,
    *,
    late_p: float,
    late_days_min: int,
    late_days_max: int,
    force_late: bool = False,
) -> datetime:
    """
    Timestamp for a realized payment.

    Late payments are shifted by the configured late-day range.
    On-time payments still get hour/minute jitter within the day.
    """
    should_be_late = force_late or rng.coin(late_p)

    if should_be_late:
        delay_days = 0
        if late_days_max > 0:
            delay_days = rng.int(late_days_min, late_days_max + 1)

        return due_ts + timedelta(
            days=delay_days,
            hours=rng.int(8, 22),
            minutes=rng.int(0, 60),
        )

    return due_ts + timedelta(
        hours=rng.int(6, 18),
        minutes=rng.int(0, 60),
    )


def _make_txn(
    *,
    txf: TransactionFactory,
    src: str,
    dst: str,
    amount: float,
    timestamp: datetime,
    channel: str,
) -> Transaction:
    return txf.make(
        TransactionDraft(
            source=src,
            destination=dst,
            amount=_round_money(amount),
            timestamp=timestamp,
            channel=channel,
        )
    )


def _resolve_terms(
    registry: PortfolioRegistry,
    event: ObligationEvent,
) -> InstallmentTerms | None:
    """
    Resolve the concrete installment-loan terms backing this obligation event.
    """
    portfolio = registry.get(event.person_id)
    if portfolio is None:
        return None

    if event.product_type == "mortgage":
        return portfolio.mortgage
    if event.product_type == "auto_loan":
        return portfolio.auto_loan
    if event.product_type == "student_loan":
        return portfolio.student_loan

    return None


def _emit_plain_event(
    *,
    rng: Rng,
    txf: TransactionFactory,
    event: ObligationEvent,
    person_acct: str,
    end_excl: datetime,
) -> Transaction | None:
    """
    Emit a non-installment obligation with light timestamp jitter only.
    """
    ts = _add_basic_jitter(rng, event.timestamp)
    if ts >= end_excl:
        return None

    if event.direction == Direction.OUTFLOW:
        src = person_acct
        dst = event.counterparty_acct
    else:
        src = event.counterparty_acct
        dst = person_acct

    return _make_txn(
        txf=txf,
        src=src,
        dst=dst,
        amount=event.amount,
        timestamp=ts,
        channel=event.channel,
    )


def _effective_prob(base: float, multiplier: float) -> float:
    return min(0.95, max(0.0, base * multiplier))


def _installment_payment_amount(
    *,
    rng: Rng,
    total_due: float,
    partial_min_frac: float,
    partial_max_frac: float,
) -> float:
    """
    Realized amount for a partial installment payment.
    """
    frac = _uniform_between(rng, partial_min_frac, partial_max_frac)
    raw_paid = total_due * frac
    paid = min(total_due, max(0.01, raw_paid))
    return _round_money(paid)


def _emit_installment_event(
    *,
    registry: PortfolioRegistry,
    state_by_key: dict[tuple[str, str], _InstallmentState],
    rng: Rng,
    txf: TransactionFactory,
    event: ObligationEvent,
    person_acct: str,
    end_excl: datetime,
) -> list[Transaction]:
    """
    Emit one scheduled installment due event as zero or one observed
    transaction depending on delinquency behavior.

    Current modeled cases:
    - missed payment -> no txn, amount carried forward
    - partial payment -> one txn for a fraction of total due
    - cure payment -> one txn that catches up all arrears
    - normal payment -> one full scheduled payment
    """
    terms = _resolve_terms(registry, event)
    if terms is None:
        txn = _emit_plain_event(
            rng=rng,
            txf=txf,
            event=event,
            person_acct=person_acct,
            end_excl=end_excl,
        )
        return [] if txn is None else [txn]

    state_key = (event.person_id, event.product_type)
    state = state_by_key.setdefault(state_key, _InstallmentState())

    scheduled_amount = _round_money(event.amount)
    total_due = _round_money(scheduled_amount + state.carry_due)

    if total_due <= 0.0:
        return []

    late_p = terms.late_p
    late_days_min = terms.late_days_min
    late_days_max = terms.late_days_max
    miss_p = terms.miss_p
    partial_p = terms.partial_p
    cure_p = terms.cure_p
    cluster_mult = terms.cluster_mult
    partial_min_frac = terms.partial_min_frac
    partial_max_frac = terms.partial_max_frac

    delinquency_mult = 1.0
    if state.delinquent_cycles > 0:
        delinquency_mult = cluster_mult ** min(state.delinquent_cycles, 2)

    effective_late_p = _effective_prob(late_p, delinquency_mult)
    effective_miss_p = _effective_prob(miss_p, delinquency_mult)
    effective_partial_p = _effective_prob(
        partial_p,
        1.0 + (0.5 * state.delinquent_cycles),
    )

    txns: list[Transaction] = []

    # Catch-up / cure payment for previously unpaid balance.
    if state.carry_due > 0.0:
        cure_boost = 1.0 + (0.25 * state.delinquent_cycles)
        effective_cure_p = min(0.98, cure_p * cure_boost)

        if rng.coin(effective_cure_p):
            cure_ts = _payment_timestamp(
                rng,
                event.timestamp,
                late_p=max(effective_late_p, 0.75),
                late_days_min=late_days_min,
                late_days_max=late_days_max,
                force_late=True,
            )
            if cure_ts < end_excl:
                txns.append(
                    _make_txn(
                        txf=txf,
                        src=person_acct,
                        dst=event.counterparty_acct,
                        amount=total_due,
                        timestamp=cure_ts,
                        channel=event.channel,
                    )
                )
                state.carry_due = 0.0
                state.delinquent_cycles = 0

            return txns

    decision_u = rng.float()

    # Missed payment: nothing posts this cycle; full due carries forward.
    if decision_u < effective_miss_p:
        state.carry_due = total_due
        state.delinquent_cycles += 1
        return []

    decision_u -= effective_miss_p

    # Partial payment: some amount posts, remainder carries forward.
    if decision_u < effective_partial_p:
        paid_amount = _installment_payment_amount(
            rng=rng,
            total_due=total_due,
            partial_min_frac=partial_min_frac,
            partial_max_frac=partial_max_frac,
        )
        unpaid_amount = _round_money(total_due - paid_amount)

        if paid_amount > 0.0:
            partial_ts = _payment_timestamp(
                rng,
                event.timestamp,
                late_p=effective_late_p,
                late_days_min=late_days_min,
                late_days_max=late_days_max,
            )
            if partial_ts < end_excl:
                txns.append(
                    _make_txn(
                        txf=txf,
                        src=person_acct,
                        dst=event.counterparty_acct,
                        amount=paid_amount,
                        timestamp=partial_ts,
                        channel=event.channel,
                    )
                )

        state.carry_due = unpaid_amount
        if unpaid_amount > 0.0:
            state.delinquent_cycles += 1
        else:
            state.delinquent_cycles = 0

        return txns

    # Full scheduled-cycle payment.
    full_ts = _payment_timestamp(
        rng,
        event.timestamp,
        late_p=effective_late_p,
        late_days_min=late_days_min,
        late_days_max=late_days_max,
    )
    if full_ts < end_excl:
        txns.append(
            _make_txn(
                txf=txf,
                src=person_acct,
                dst=event.counterparty_acct,
                amount=scheduled_amount,
                timestamp=full_ts,
                channel=event.channel,
            )
        )

    # If arrears still remain from prior cycles, delinquency persists.
    if state.carry_due > 0.0:
        state.delinquent_cycles += 1
    else:
        state.delinquent_cycles = 0

    return txns


def emit(
    registry: PortfolioRegistry,
    *,
    start: datetime,
    end_excl: datetime,
    primary_accounts: dict[str, str],
    rng: Rng,
    txf: TransactionFactory,
) -> list[Transaction]:
    """
    Convert all scheduled obligation events into Transactions.

    Skips events where the person has no primary account
    (external-only or unresolvable).

    Installment loans get delinquency-aware realization logic.
    Other obligations keep the original light-jitter behavior.
    """
    txns: list[Transaction] = []
    state_by_key: dict[tuple[str, str], _InstallmentState] = {}

    for event in registry.all_events(start, end_excl):
        person_acct = primary_accounts.get(event.person_id)
        if not person_acct:
            continue

        if (
            event.direction == Direction.OUTFLOW
            and event.product_type in _LOAN_PRODUCT_TYPES
        ):
            txns.extend(
                _emit_installment_event(
                    registry=registry,
                    state_by_key=state_by_key,
                    rng=rng,
                    txf=txf,
                    event=event,
                    person_acct=person_acct,
                    end_excl=end_excl,
                )
            )
            continue

        txn = _emit_plain_event(
            rng=rng,
            txf=txf,
            event=event,
            person_acct=person_acct,
            end_excl=end_excl,
        )
        if txn is not None:
            txns.append(txn)

    txns.sort(key=lambda t: t.timestamp)
    return txns
