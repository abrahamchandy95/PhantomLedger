from pathlib import Path

from common.config import GenerationConfig, default_config
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
from common.rng import Rng
from emit.tg_csv import write_csv
from entities.accounts import (
    build_account_rows,
    build_has_account_rows,
    generate_accounts,
)
from entities.people import build_person_rows, generate_people
from entities.pii import (
    build_email_rows,
    build_has_email_rows,
    build_has_phone_rows,
    build_phone_rows,
    generate_pii,
)
from infra.devices import build_device_rows, generate_devices
from infra.ipaddrs import build_ip_rows, generate_ipaddrs
from infra.txn_infra import TxnInfraAssigner
from transfers.aggregate import emit_transfer_outputs
from transfers.fraud import FraudInjectionKnobs, inject_fraud_transfers
from transfers.legit import generate_legit_transfers


def generate_all(cfg: GenerationConfig | None = None) -> None:
    """
    End-to-end generation pipeline.

    Order:
      people -> accounts -> pii -> devices -> ipaddrs -> legit transfers
      -> fraud injection -> aggregate + emit -> write all other CSVs
    """
    if cfg is None:
        cfg = default_config()
    else:
        cfg.validate()

    out_dir: Path = cfg.output.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    rng = Rng.from_seed(cfg.population.seed)

    # -----------------------------
    # Entities
    # -----------------------------
    people = generate_people(cfg.population, cfg.fraud, rng)
    people_rows = build_person_rows(people)

    accounts = generate_accounts(cfg.accounts, rng, people)
    account_rows = build_account_rows(accounts)
    has_account_rows = build_has_account_rows(accounts)

    pii = generate_pii(people.people, rng)
    phone_rows = build_phone_rows(pii)
    email_rows = build_email_rows(pii)
    has_phone_rows = build_has_phone_rows(people.people, pii)
    has_email_rows = build_has_email_rows(people.people, pii)

    # -----------------------------
    # Infrastructure
    # -----------------------------
    devices = generate_devices(cfg.window, cfg.fraud, rng, people)
    device_rows = build_device_rows(devices)
    has_used_rows = devices.has_used_rows

    ipdata = generate_ipaddrs(cfg.window, cfg.fraud, rng, people)
    ip_rows = build_ip_rows(ipdata)
    has_ip_rows = ipdata.has_ip_rows

    infra_assigner = TxnInfraAssigner.build(
        cfg.infra,
        rng,
        acct_owner=accounts.acct_owner,
        person_devices=devices.person_devices,
        person_ips=ipdata.person_ips,
    )

    # -----------------------------
    # Transfers
    # -----------------------------
    legit = generate_legit_transfers(
        cfg.window,
        cfg.population,
        cfg.hubs,
        cfg.personas,
        cfg.graph,
        cfg.events,
        cfg.recurring,
        cfg.balances,
        rng,
        accounts,
        infra=infra_assigner,
    )
    base_txns = legit.txns

    fraud_res = inject_fraud_transfers(
        cfg.fraud,
        cfg.window,
        FraudInjectionKnobs(),
        rng,
        people,
        accounts,
        base_txns,
        infra=infra_assigner,
    )
    all_txns = fraud_res.txns

    unique_has_paid_edges = emit_transfer_outputs(
        out_dir, all_txns, cfg.output.emit_raw_ledger
    )

    # -----------------------------
    # Emit non-transfer CSVs
    # -----------------------------
    write_csv(out_dir / PERSON.filename, PERSON.header, people_rows)
    write_csv(out_dir / ACCOUNTNUMBER.filename, ACCOUNTNUMBER.header, account_rows)
    write_csv(out_dir / PHONE.filename, PHONE.header, phone_rows)
    write_csv(out_dir / EMAIL.filename, EMAIL.header, email_rows)
    write_csv(out_dir / DEVICE.filename, DEVICE.header, device_rows)
    write_csv(out_dir / IPADDRESS.filename, IPADDRESS.header, ip_rows)

    write_csv(out_dir / HAS_ACCOUNT.filename, HAS_ACCOUNT.header, has_account_rows)
    write_csv(out_dir / HAS_PHONE.filename, HAS_PHONE.header, has_phone_rows)
    write_csv(out_dir / HAS_EMAIL.filename, HAS_EMAIL.header, has_email_rows)
    write_csv(out_dir / HAS_USED.filename, HAS_USED.header, has_used_rows)
    write_csv(out_dir / HAS_IP.filename, HAS_IP.header, has_ip_rows)

    # -----------------------------
    # Summary
    # -----------------------------
    total_before_fraud = len(base_txns)
    total_after_fraud = len(all_txns)
    illicit = sum(1 for t in all_txns if t.is_fraud == 1)
    ratio = illicit / max(1, total_after_fraud)

    print(f"Wrote outputs to: {out_dir}")
    print(f"People: {len(people.people)}  Accounts: {len(accounts.accounts)}")
    print(f"Transactions: {total_after_fraud} (before fraud: {total_before_fraud})")
    print(
        f"Illicit txns: {illicit} ({ratio:.6%})  Injected: {fraud_res.injected_count}"
    )
    print(f"Unique HAS_PAID edges: {unique_has_paid_edges}")
