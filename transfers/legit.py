from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import cast

from common.config import (
    BalancesConfig,
    EventsConfig,
    GraphConfig,
    HubsConfig,
    PersonasConfig,
    PopulationConfig,
    RecurringConfig,
    WindowConfig,
)
from common.rng import Rng
from common.timeutil import iter_month_starts
from common.types import Txn
from entities.accounts import AccountsData
from entities.personas import assign_personas
from infra.txn_infra import TxnInfraAssigner
from math_models.amounts import bill_amount, salary_amount
from math_models.graph import build_neighbor_graph
from relationships.recurring import (
    EmploymentState,
    LeaseState,
    advance_employment,
    advance_lease,
    init_employment,
    init_lease,
    rent_at,
    salary_at,
)
from transfers.balances import init_balances, try_transfer
from transfers.day_to_day import (
    build_day_to_day_context,
    generate_day_to_day_superposition,
)


@dataclass(frozen=True, slots=True)
class LegitTransfers:
    txns: list[Txn]
    hub_accounts: list[str]
    biller_accounts: list[str]


def _select_hub_accounts(
    pop: PopulationConfig,
    hubs: HubsConfig,
    rng: Rng,
    accounts: AccountsData,
) -> list[str]:
    """
    Pick hub accounts (employers/billers/high-volume entities).
    """
    persons = list(accounts.person_accounts.keys())
    if not persons:
        return []

    n_hubs = int(pop.persons * hubs.hub_fraction)
    if n_hubs < 1:
        n_hubs = 1
    if n_hubs > len(persons):
        n_hubs = len(persons)

    hub_people = rng.choice_k(persons, n_hubs, replace=False)
    return [accounts.person_accounts[p][0] for p in hub_people]


def _supports_txn_fields() -> tuple[bool, bool, bool]:
    """
    Returns (supports_device_id, supports_ip_address, supports_channel).
    """
    fields_obj: object = getattr(Txn, "__dataclass_fields__", {})
    if isinstance(fields_obj, dict):
        fields = cast(dict[str, object], fields_obj)
        return ("device_id" in fields, "ip_address" in fields, "channel" in fields)
    return (False, False, False)


_SUPPORTS_DEVICE, _SUPPORTS_IP, _SUPPORTS_CHANNEL = _supports_txn_fields()


def _make_txn(
    rng: Rng,
    infra: TxnInfraAssigner | None,
    *,
    src: str,
    dst: str,
    amt: float,
    ts: datetime,
    is_fraud: int,
    ring_id: int,
    channel: str,
) -> Txn:
    """
    Create Txn and optionally stamp device/ip/channel if supported by Txn.
    Uses positional args to keep basedpyright happy.
    """
    device_id: str | None = None
    ip_address: str | None = None
    if infra is not None:
        device_id, ip_address = infra.pick_for_src(rng, src)

    if _SUPPORTS_DEVICE or _SUPPORTS_IP or _SUPPORTS_CHANNEL:
        if _SUPPORTS_DEVICE and _SUPPORTS_IP and _SUPPORTS_CHANNEL:
            return Txn(
                src,
                dst,
                float(amt),
                ts,
                int(is_fraud),
                int(ring_id),
                device_id,
                ip_address,
                channel,
            )
        if _SUPPORTS_DEVICE and _SUPPORTS_IP:
            return Txn(
                src,
                dst,
                float(amt),
                ts,
                int(is_fraud),
                int(ring_id),
                device_id,
                ip_address,
            )
        if _SUPPORTS_DEVICE and _SUPPORTS_CHANNEL:
            return Txn(
                src,
                dst,
                float(amt),
                ts,
                int(is_fraud),
                int(ring_id),
                device_id,
                None,
                channel,
            )
        if _SUPPORTS_IP and _SUPPORTS_CHANNEL:
            return Txn(
                src,
                dst,
                float(amt),
                ts,
                int(is_fraud),
                int(ring_id),
                None,
                ip_address,
                channel,
            )
        if _SUPPORTS_DEVICE:
            return Txn(src, dst, float(amt), ts, int(is_fraud), int(ring_id), device_id)
        if _SUPPORTS_IP:
            return Txn(
                src, dst, float(amt), ts, int(is_fraud), int(ring_id), None, ip_address
            )
        if _SUPPORTS_CHANNEL:
            return Txn(
                src,
                dst,
                float(amt),
                ts,
                int(is_fraud),
                int(ring_id),
                None,
                None,
                channel,
            )

    return Txn(src, dst, float(amt), ts, int(is_fraud), int(ring_id))


def generate_legit_transfers(
    window: WindowConfig,
    pop: PopulationConfig,
    hubs: HubsConfig,
    personas: PersonasConfig,
    graph_cfg: GraphConfig,
    events: EventsConfig,
    recurring: RecurringConfig,
    balances_cfg: BalancesConfig,
    rng: Rng,
    accounts: AccountsData,
    *,
    infra: TxnInfraAssigner | None = None,
    persona_names_override: list[str] | None = None,
) -> LegitTransfers:
    """
    Legitimate ledger generation.
    """
    start_date = window.start_date()
    days = int(window.days)
    seed = int(pop.seed)

    all_accounts = accounts.accounts
    if not all_accounts:
        return LegitTransfers(txns=[], hub_accounts=[], biller_accounts=[])

    # -----------------------------
    # Hubs / billers / employers
    # -----------------------------
    hub_accounts = _select_hub_accounts(pop, hubs, rng, accounts)
    hub_set = set(hub_accounts)

    biller_accounts = hub_accounts if hub_accounts else [all_accounts[0]]
    employers = (
        hub_accounts[: max(1, len(hub_accounts) // 5)]
        if hub_accounts
        else [all_accounts[0]]
    )

    # -----------------------------
    # Paydays within sim window
    # -----------------------------
    paydays = [d for d in iter_month_starts(start_date, days) if d >= start_date]

    persons = list(accounts.person_accounts.keys())
    primary_acct_for_person: dict[str, str] = {
        p: accounts.person_accounts[p][0] for p in persons
    }

    txns: list[Txn] = []

    # -----------------------------
    # Personas + graph + day-to-day context
    # -----------------------------
    persona_for_person_map = assign_personas(personas, rng, persons)

    graph = build_neighbor_graph(
        graph_cfg,
        rng,
        accounts=all_accounts,
        acct_owner=accounts.acct_owner,
        hub_accounts=set(hub_accounts),
    )

    ctx = build_day_to_day_context(
        events,
        accounts=all_accounts,
        hub_accounts=hub_accounts,
        biller_accounts=biller_accounts,
        persona_for_person=persona_for_person_map,
        acct_owner=accounts.acct_owner,
        persona_names_override=persona_names_override,
    )

    # -----------------------------
    # Optional balance constraints
    # -----------------------------
    book = None
    if balances_cfg.enable_balance_constraints:
        book = init_balances(
            balances_cfg,
            rng,
            accounts=all_accounts,
            acct_index=ctx.acct_index,
            hub_set_idx=ctx.hub_set_idx,
            persona_for_acct=ctx.persona_for_acct,
            persona_names=ctx.persona_names,
        )

    def _append_txn(t: Txn) -> None:
        if book is None:
            txns.append(t)
            return
        if try_transfer(book, t.src_acct, t.dst_acct, float(t.amount)):
            txns.append(t)

    # -----------------------------
    # Salaries
    # -----------------------------
    salary_fraction = float(recurring.salary_fraction)
    salary_candidates = [
        p for p in persons if primary_acct_for_person[p] not in hub_set
    ]

    salary_people_n = int(salary_fraction * len(salary_candidates))
    if salary_people_n > len(salary_candidates):
        salary_people_n = len(salary_candidates)

    salary_people = (
        rng.choice_k(salary_candidates, salary_people_n, replace=False)
        if salary_people_n > 0
        else []
    )

    employment: dict[str, EmploymentState] = {}

    def _base_salary_draw() -> float:
        return float(salary_amount(rng))

    for p in salary_people:
        employment[p] = init_employment(
            recurring,
            seed,
            rng,
            person_id=p,
            start_date=start_date,
            employers=employers,
            base_salary_sampler=_base_salary_draw,
        )

    for pd in paydays:
        for p in salary_people:
            st = employment[p]
            while pd >= st.end:
                st = advance_employment(
                    recurring,
                    seed,
                    rng,
                    person_id=p,
                    now=st.end,
                    employers=employers,
                    prev=st,
                )
            employment[p] = st

            dst_acct = primary_acct_for_person[p]
            src_acct = st.employer_acct
            ts = pd + timedelta(hours=rng.int(8, 18), minutes=rng.int(0, 60))
            amt = salary_at(recurring, seed, person_id=p, state=st, pay_date=pd)

            _append_txn(
                _make_txn(
                    rng,
                    infra,
                    src=src_acct,
                    dst=dst_acct,
                    amt=amt,
                    ts=ts,
                    is_fraud=0,
                    ring_id=-1,
                    channel="salary",
                )
            )

    # -----------------------------
    # Rent
    # -----------------------------
    rent_fraction = float(recurring.rent_fraction)

    rent_payers = [a for a in primary_acct_for_person.values() if a not in hub_set]
    rent_n = int(rent_fraction * len(rent_payers))
    if rent_n > len(rent_payers):
        rent_n = len(rent_payers)

    rent_active = rng.choice_k(rent_payers, rent_n, replace=False) if rent_n > 0 else []

    leases: dict[str, LeaseState] = {}

    def _base_rent_draw() -> float:
        return float(bill_amount(rng))

    for a in rent_active:
        leases[a] = init_lease(
            recurring,
            seed,
            rng,
            payer_acct=a,
            start_date=start_date,
            landlords=biller_accounts,
            base_rent_sampler=_base_rent_draw,
        )

    for pd in paydays:
        for a in rent_active:
            st = leases[a]
            while pd >= st.end:
                st = advance_lease(
                    recurring,
                    seed,
                    rng,
                    payer_acct=a,
                    now=st.end,
                    landlords=biller_accounts,
                    prev=st,
                    reset_rent_sampler=_base_rent_draw,
                )
            leases[a] = st

            landlord = st.landlord_acct
            ts = pd + timedelta(
                days=rng.int(0, 5), hours=rng.int(7, 22), minutes=rng.int(0, 60)
            )
            amt = rent_at(recurring, seed, payer_acct=a, state=st, pay_date=pd)

            _append_txn(
                _make_txn(
                    rng,
                    infra,
                    src=a,
                    dst=landlord,
                    amt=amt,
                    ts=ts,
                    is_fraud=0,
                    ring_id=-1,
                    channel="rent",
                )
            )

    # -----------------------------
    # Day-to-day
    # -----------------------------
    day_txns = generate_day_to_day_superposition(
        events,
        rng,
        start_date=start_date,
        days=days,
        ctx=ctx,
        graph=graph,
        infra=infra,
    )
    for t in day_txns:
        _append_txn(t)

    return LegitTransfers(
        txns=txns, hub_accounts=hub_accounts, biller_accounts=biller_accounts
    )
