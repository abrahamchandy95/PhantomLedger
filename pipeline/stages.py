from collections.abc import Iterable
from datetime import datetime

import emit
from common.schema import (
    ACCOUNTNUMBER,
    DEVICE,
    EMAIL,
    HAS_ACCOUNT,
    HAS_EMAIL,
    HAS_IP,
    HAS_PHONE,
    HAS_USED,
    IPADDRESS,
    PERSON,
    PHONE,
    CsvSpec,
)
from common.types import Txn
from entities.accounts import generate_accounts, with_extra_accounts
from entities.credit_cards import attach_credit_cards
from entities.merchants import generate_merchants
from entities.people import generate_people
from entities.personas import assign_personas
from entities.pii import generate_pii
from infra.devices import generate_devices
from infra.ipaddrs import generate_ipaddrs
from infra.txn_infra import TxnInfraAssigner
from pipeline.state import GenerationState
from transfers.aggregate import emit_transfer_outputs
from transfers.fraud import FraudInjectionKnobs, inject_fraud_transfers
from transfers.legit import generate_legit_transfers


type TxnSortKey = tuple[datetime, str, str, float, int, int, str, str, str]


def _txn_sort_key(t: Txn) -> TxnSortKey:
    return (
        t.ts,
        t.src_acct,
        t.dst_acct,
        float(t.amount),
        int(t.is_fraud),
        int(t.ring_id),
        t.channel or "",
        t.device_id or "",
        t.ip_address or "",
    )


def build_entities(st: GenerationState) -> None:
    cfg = st.cfg
    rng = st.rng

    st.people = generate_people(cfg.population, cfg.fraud, rng)
    st.accounts = generate_accounts(cfg.accounts, rng, st.people)
    st.pii = generate_pii(st.people.people, rng)

    # Merchant ecosystem
    st.merchants = generate_merchants(cfg.merchants, cfg.population, rng)

    # Add in-bank merchants (M...) into internal account universe
    if st.merchants.in_bank_accounts:
        st.accounts = with_extra_accounts(st.accounts, st.merchants.in_bank_accounts)

    # Compute personas once
    persons = list(st.accounts.person_accounts.keys())
    st.persona_for_person = assign_personas(cfg.personas, rng, persons)

    # Attach credit card accounts before infra is built
    st.accounts, st.credit_cards = attach_credit_cards(
        cfg.credit,
        rng,
        base_seed=cfg.population.seed,
        accounts=st.accounts,
        persons=persons,
        persona_for_person=st.persona_for_person,
    )


def build_infra(st: GenerationState) -> None:
    cfg = st.cfg
    rng = st.rng

    assert st.people is not None
    assert st.accounts is not None

    st.devices = generate_devices(cfg.window, cfg.fraud, rng, st.people)
    st.ipdata = generate_ipaddrs(cfg.window, cfg.fraud, rng, st.people)

    st.infra = TxnInfraAssigner.build(
        cfg.infra,
        rng,
        acct_owner=st.accounts.acct_owner,
        person_devices=st.devices.person_devices,
        person_ips=st.ipdata.person_ips,
    )


def build_transfers(st: GenerationState) -> None:
    cfg = st.cfg
    rng = st.rng

    assert st.people is not None
    assert st.accounts is not None
    assert st.merchants is not None
    assert st.persona_for_person is not None

    st.legit = generate_legit_transfers(
        cfg.window,
        cfg.population,
        cfg.hubs,
        cfg.personas,
        cfg.graph,
        cfg.events,
        cfg.recurring,
        cfg.balances,
        cfg.merchants,
        rng,
        st.accounts,
        merchants=st.merchants,
        infra=st.infra,
        persona_for_person_override=st.persona_for_person,
        family_cfg=cfg.family,
        credit_cfg=cfg.credit,
        credit_cards=st.credit_cards,
    )
    st.base_txns = st.legit.txns

    st.fraud = inject_fraud_transfers(
        cfg.fraud,
        cfg.window,
        FraudInjectionKnobs(),
        rng,
        st.people,
        st.accounts,
        st.base_txns,
        biller_accounts=st.legit.biller_accounts,
        employers=st.legit.employers,
        infra=st.infra,
    )

    # Canonical chronological order for all downstream consumers.
    st.all_txns = sorted(st.fraud.txns, key=_txn_sort_key)


def emit_outputs(st: GenerationState) -> None:
    cfg = st.cfg
    out_dir = st.out_dir

    assert st.people is not None
    assert st.accounts is not None
    assert st.pii is not None
    assert st.devices is not None
    assert st.ipdata is not None
    assert st.all_txns is not None

    st.unique_has_paid_edges = emit_transfer_outputs(
        out_dir, st.all_txns, cfg.output.emit_raw_ledger
    )

    tables: list[tuple[CsvSpec, Iterable[emit.CsvRow]]] = [
        (PERSON, emit.iter_person_rows(st.people)),
        (ACCOUNTNUMBER, emit.iter_account_rows(st.accounts)),
        (PHONE, emit.iter_phone_rows(st.pii)),
        (EMAIL, emit.iter_email_rows(st.pii)),
        (DEVICE, emit.iter_device_rows(st.devices)),
        (IPADDRESS, emit.iter_ip_rows(st.ipdata)),
        (HAS_ACCOUNT, emit.iter_has_account_rows(st.accounts)),
        (HAS_PHONE, emit.iter_has_phone_rows(st.people.people, st.pii)),
        (HAS_EMAIL, emit.iter_has_email_rows(st.people.people, st.pii)),
        (HAS_USED, emit.iter_has_used_rows(st.devices)),
        (HAS_IP, emit.iter_has_ip_rows(st.ipdata)),
    ]

    for spec, rows in tables:
        emit.write_csv(out_dir / spec.filename, spec.header, rows)

    if st.merchants is not None:
        emit.write_csv(
            out_dir / "merchants.csv",
            ("merchant_id", "counterparty_acct", "category", "weight", "in_bank"),
            emit.iter_merchants_rows(st.merchants),
        )
        emit.write_csv(
            out_dir / "external_accounts.csv",
            ("account_id", "kind", "category"),
            emit.iter_external_accounts_rows(st.merchants),
        )


def print_summary(st: GenerationState) -> None:
    assert st.people is not None
    assert st.accounts is not None
    assert st.base_txns is not None
    assert st.all_txns is not None
    assert st.fraud is not None

    total_before = len(st.base_txns)
    total_after = len(st.all_txns)
    illicit = sum(1 for t in st.all_txns if t.is_fraud == 1)
    ratio = illicit / max(1, total_after)

    print(f"Wrote outputs to: {st.out_dir}")
    print(f"People: {len(st.people.people)}  Accounts: {len(st.accounts.accounts)}")
    print(f"Transactions: {total_after} (before fraud: {total_before})")
    print(f"Illicit txns: {illicit} ({ratio:.6%})  Injected: {st.fraud.injected_count}")
    print(f"Unique HAS_PAID edges: {st.unique_has_paid_edges}")
