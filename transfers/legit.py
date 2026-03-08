from dataclasses import dataclass
from datetime import timedelta

import numpy as np

from common.config import (
    BalancesConfig,
    CreditConfig,
    EventsConfig,
    FamilyConfig,
    GraphConfig,
    HubsConfig,
    MerchantsConfig,
    PersonasConfig,
    PopulationConfig,
    RecurringConfig,
    WindowConfig,
)
from common.rng import Rng
from common.temporal import iter_month_starts
from common.types import Txn
from entities.accounts import AccountsData
from entities.credit_cards import CreditCardData
from entities.merchants import MerchantData
from entities.personas import assign_personas
from infra.txn_infra import TxnInfraAssigner
from math_models.amounts import bill_amount, salary_amount
from relationships.family import generate_family
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
from relationships.social import build_social_graph
from transfers.balances import init_balances, try_transfer
from transfers.credit_cards import generate_credit_card_lifecycle_txns
from transfers.day_to_day import (
    build_day_to_day_context,
    generate_day_to_day_superposition,
)
from transfers.family_transfers import generate_family_transfers
from transfers.txns import TxnFactory, TxnSpec


@dataclass(frozen=True, slots=True)
class LegitTransfers:
    txns: list[Txn]
    hub_accounts: list[str]
    biller_accounts: list[str]
    employers: list[str]


def _select_hub_accounts(
    pop: PopulationConfig,
    hubs: HubsConfig,
    rng: Rng,
    accounts: AccountsData,
) -> list[str]:
    persons = list(accounts.person_accounts.keys())
    if not persons:
        return []

    n_hubs = int(pop.persons * hubs.hub_fraction)
    n_hubs = max(1, min(n_hubs, len(persons)))

    hub_people = rng.choice_k(persons, n_hubs, replace=False)
    return [accounts.person_accounts[p][0] for p in hub_people]


def _persona_for_acct_array(
    *,
    accounts_list: list[str],
    acct_owner: dict[str, str],
    persona_for_person: dict[str, str],
    persona_names: list[str],
) -> np.ndarray:
    persona_id_for_name = {n: i for i, n in enumerate(persona_names)}
    salaried_id = int(persona_id_for_name.get("salaried", 0))

    out = np.empty(len(accounts_list), dtype=np.int8)
    for i, a in enumerate(accounts_list):
        p = acct_owner.get(a)
        pname = persona_for_person.get(p, "salaried") if p is not None else "salaried"
        out[i] = int(persona_id_for_name.get(pname, salaried_id))
    return out


def generate_legit_transfers(
    window: WindowConfig,
    pop: PopulationConfig,
    hubs: HubsConfig,
    personas: PersonasConfig,
    graph_cfg: GraphConfig,
    events: EventsConfig,
    recurring: RecurringConfig,
    balances_cfg: BalancesConfig,
    merchants_cfg: MerchantsConfig,
    rng: Rng,
    accounts: AccountsData,
    *,
    merchants: MerchantData,
    infra: TxnInfraAssigner | None = None,
    persona_names_override: list[str] | None = None,
    persona_for_person_override: dict[str, str] | None = None,
    family_cfg: FamilyConfig | None = None,
    credit_cfg: CreditConfig | None = None,
    credit_cards: CreditCardData | None = None,
) -> LegitTransfers:
    start_date = window.start_date()
    days = int(window.days)
    seed = int(pop.seed)

    all_accounts = accounts.accounts
    if not all_accounts:
        return LegitTransfers(
            txns=[],
            hub_accounts=[],
            biller_accounts=[],
            employers=[],
        )

    txf = TxnFactory(rng=rng, infra=infra)

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

    issuer_acct = hub_accounts[0] if hub_accounts else all_accounts[0]

    # -----------------------------
    # Paydays within sim window
    # -----------------------------
    paydays = [d for d in iter_month_starts(start_date, days) if d >= start_date]

    persons = list(accounts.person_accounts.keys())
    primary_acct_for_person: dict[str, str] = {
        p: accounts.person_accounts[p][0]
        for p in persons
        if accounts.person_accounts[p]
    }

    # -----------------------------
    # Personas
    # -----------------------------
    persona_for_person_map = persona_for_person_override or assign_personas(
        personas, rng, persons
    )

    persona_names = persona_names_override or [
        "student",
        "retired",
        "freelancer",
        "smallbiz",
        "hnw",
        "salaried",
    ]

    # -----------------------------
    # Credit enablement flags
    # -----------------------------
    cc_enabled = (
        credit_cfg is not None
        and credit_cfg.enable_credit_cards
        and credit_cards is not None
        and bool(credit_cards.card_for_person)
    )

    # -----------------------------
    # Optional balance constraints
    # -----------------------------
    book = None
    if balances_cfg.enable_balance_constraints:
        acct_index = {a: i for i, a in enumerate(all_accounts)}
        hub_set_idx = {acct_index[a] for a in hub_accounts if a in acct_index}

        persona_for_acct = _persona_for_acct_array(
            accounts_list=all_accounts,
            acct_owner=accounts.acct_owner,
            persona_for_person=persona_for_person_map,
            persona_names=persona_names,
        )

        book = init_balances(
            balances_cfg,
            rng,
            accounts=all_accounts,
            acct_index=acct_index,
            hub_set_idx=hub_set_idx,
            persona_for_acct=persona_for_acct,
            persona_names=persona_names,
        )

        if cc_enabled:
            assert credit_cards is not None
            for card, lim in credit_cards.limit_by_card.items():
                idx = book.acct_index.get(card)
                if idx is None:
                    continue
                book.balances[idx] = 0.0
                book.overdraft_limit[idx] = float(lim)

    txns: list[Txn] = []

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
        p for p in persons if primary_acct_for_person.get(p) not in hub_set
    ]

    salary_people_n = min(
        int(salary_fraction * len(salary_candidates)), len(salary_candidates)
    )
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

            dst_acct = primary_acct_for_person.get(p)
            if dst_acct is None:
                continue

            ts = pd + timedelta(hours=rng.int(8, 18), minutes=rng.int(0, 60))
            amt = salary_at(recurring, seed, person_id=p, state=st, pay_date=pd)

            _append_txn(
                txf.make(
                    TxnSpec(
                        src=st.employer_acct,
                        dst=dst_acct,
                        amt=amt,
                        ts=ts,
                        channel="salary",
                        is_fraud=0,
                        ring_id=-1,
                    )
                )
            )

    # -----------------------------
    # Rent
    # -----------------------------
    rent_fraction = float(recurring.rent_fraction)
    rent_payers = [a for a in primary_acct_for_person.values() if a not in hub_set]

    rent_n = min(int(rent_fraction * len(rent_payers)), len(rent_payers))
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

            ts = pd + timedelta(
                days=rng.int(0, 5), hours=rng.int(7, 22), minutes=rng.int(0, 60)
            )
            amt = rent_at(recurring, seed, payer_acct=a, state=st, pay_date=pd)

            _append_txn(
                txf.make(
                    TxnSpec(
                        src=a,
                        dst=st.landlord_acct,
                        amt=amt,
                        ts=ts,
                        channel="rent",
                        is_fraud=0,
                        ring_id=-1,
                    )
                )
            )

    # -----------------------------
    # Day-to-day social graph
    # -----------------------------
    # We keep SocialGraph, but now parameterize it directly from GraphConfig.
    k_contacts = max(3, min(24, int(graph_cfg.graph_k_neighbors)))

    # Interpreted at person level: "household/local neighborhood preference"
    local_contact_p = float(graph_cfg.graph_intra_household_p)

    # Smaller cross-community movement when local preference is high.
    cross_p = max(0.01, min(0.25, 1.0 - local_contact_p))

    # Scale community sizes with contact width so the graph stays sparse/local.
    community_min = max(20, 6 * k_contacts)
    community_max = max(community_min + 20, 24 * k_contacts)

    hub_people = {
        accounts.acct_owner[a] for a in hub_accounts if a in accounts.acct_owner
    }

    social = build_social_graph(
        rng,
        seed=seed,
        people=persons,
        k_contacts=k_contacts,
        community_min=community_min,
        community_max=community_max,
        cross_community_p=cross_p,
        local_contact_p=local_contact_p,
        hub_people=hub_people,
        hub_weight_boost=float(graph_cfg.graph_hub_weight_boost),
        attractiveness_sigma=float(graph_cfg.graph_attractiveness_sigma),
        edge_weight_gamma_shape=float(graph_cfg.graph_edge_weight_gamma_shape),
    )

    ctx = build_day_to_day_context(
        events,
        merchants_cfg,
        rng,
        start_date=start_date,
        days=days,
        persons=persons,
        primary_acct_for_person=primary_acct_for_person,
        persona_for_person=persona_for_person_map,
        merchants=merchants,
        social=social,
        base_seed=seed,
    )

    card_map = (
        credit_cards.card_for_person
        if cc_enabled and credit_cards is not None
        else None
    )

    day_txns = generate_day_to_day_superposition(
        events,
        merchants_cfg,
        rng,
        start_date=start_date,
        days=days,
        ctx=ctx,
        infra=infra,
        credit_cfg=credit_cfg if cc_enabled else None,
        card_for_person=card_map,
    )

    for t in day_txns:
        _append_txn(t)

    # -----------------------------
    # Family flows
    # -----------------------------
    if family_cfg is not None:
        fam = generate_family(
            family_cfg,
            rng,
            base_seed=seed,
            persons=persons,
            persona_for_person=persona_for_person_map,
        )
        fam_txns = generate_family_transfers(
            window,
            family_cfg,
            rng,
            base_seed=seed,
            family=fam,
            persona_for_person=persona_for_person_map,
            primary_acct_for_person=primary_acct_for_person,
            merchants=merchants,
            infra_factory=txf,
        )
        for ft in fam_txns:
            _append_txn(ft)

    # -----------------------------
    # Credit card lifecycle
    # -----------------------------
    if cc_enabled:
        assert credit_cfg is not None
        assert credit_cards is not None

        cc_txns = generate_credit_card_lifecycle_txns(
            window,
            credit_cfg,
            rng,
            base_seed=seed,
            cards=credit_cards,
            existing_txns=txns,
            primary_acct_for_person=primary_acct_for_person,
            issuer_acct=issuer_acct,
            txf=txf,
        )
        for ct in cc_txns:
            _append_txn(ct)

    return LegitTransfers(
        txns=txns,
        hub_accounts=hub_accounts,
        biller_accounts=biller_accounts,
        employers=employers,
    )
