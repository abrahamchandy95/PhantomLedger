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
)
from emit.tg_csv import write_csv
from entities.accounts import (
    generate_accounts,
    iter_account_rows,
    iter_has_account_rows,
    with_extra_accounts,
)
from entities.people import generate_people, iter_person_rows
from entities.pii import (
    generate_pii,
    iter_email_rows,
    iter_has_email_rows,
    iter_has_phone_rows,
    iter_phone_rows,
)
from entities.merchants import (
    generate_merchants,
    iter_merchants_rows,
    iter_external_accounts_rows,
)
from infra.devices import generate_devices, iter_device_rows
from infra.ipaddrs import generate_ipaddrs, iter_ip_rows
from infra.txn_infra import TxnInfraAssigner
from pipeline.state import GenerationState
from transfers.aggregate import emit_transfer_outputs
from transfers.fraud import FraudInjectionKnobs, inject_fraud_transfers
from transfers.legit import generate_legit_transfers


def build_entities(st: GenerationState) -> None:
    cfg = st.cfg
    rng = st.rng

    st.people = generate_people(cfg.population, cfg.fraud, rng)
    st.accounts = generate_accounts(cfg.accounts, rng, st.people)
    st.pii = generate_pii(st.people.people, rng)

    # NEW: merchant ecosystem
    st.merchants = generate_merchants(cfg.merchants, cfg.population, rng)

    # Add in-bank merchants (M...) into internal account universe
    if st.merchants.in_bank_accounts:
        st.accounts = with_extra_accounts(st.accounts, st.merchants.in_bank_accounts)


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

    st.legit = generate_legit_transfers(
        cfg.window,
        cfg.population,
        cfg.hubs,
        cfg.personas,
        cfg.graph,
        cfg.events,
        cfg.recurring,
        cfg.balances,
        cfg.merchants,  # NEW
        rng,
        st.accounts,
        merchants=st.merchants,  # NEW
        infra=st.infra,
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
        biller_accounts=st.legit.biller_accounts,  # NEW
        employers=st.legit.employers,  # NEW
        infra=st.infra,
    )
    st.all_txns = st.fraud.txns


def emit_outputs(st: GenerationState) -> None:
    cfg = st.cfg
    out_dir = st.out_dir

    assert st.people is not None
    assert st.accounts is not None
    assert st.pii is not None
    assert st.devices is not None
    assert st.ipdata is not None
    assert st.all_txns is not None

    # Transfers (HAS_PAID always, optional raw ledger)
    st.unique_has_paid_edges = emit_transfer_outputs(
        out_dir, st.all_txns, cfg.output.emit_raw_ledger
    )

    # Non-transfer CSVs (TigerGraph-ready core)
    tables = [
        (PERSON, iter_person_rows(st.people)),
        (ACCOUNTNUMBER, iter_account_rows(st.accounts)),
        (PHONE, iter_phone_rows(st.pii)),
        (EMAIL, iter_email_rows(st.pii)),
        (DEVICE, iter_device_rows(st.devices)),
        (IPADDRESS, iter_ip_rows(st.ipdata)),
        (HAS_ACCOUNT, iter_has_account_rows(st.accounts)),
        (HAS_PHONE, iter_has_phone_rows(st.people.people, st.pii)),
        (HAS_EMAIL, iter_has_email_rows(st.people.people, st.pii)),
        (HAS_USED, st.devices.has_used_rows),
        (HAS_IP, st.ipdata.has_ip_rows),
    ]

    for spec, rows in tables:
        write_csv(out_dir / spec.filename, spec.header, rows)

    # NEW: export merchant + external metadata (generic ledger extras)
    if st.merchants is not None:
        write_csv(
            out_dir / "merchants.csv",
            ("merchant_id", "counterparty_acct", "category", "weight", "in_bank"),
            iter_merchants_rows(st.merchants),
        )
        write_csv(
            out_dir / "external_accounts.csv",
            ("account_id", "kind", "category"),
            iter_external_accounts_rows(st.merchants),
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
