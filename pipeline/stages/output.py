from collections.abc import Iterable

import common.schema as schema
import emit
from pipeline.state import GenerationState
from transfers.aggregate import emit_transfer_outputs


def emit_outputs(st: GenerationState) -> None:
    cfg = st.cfg
    out_dir = st.out_dir

    entities = st.require_entities()
    infra_state = st.require_infra()
    transfers = st.require_transfers()

    st.unique_has_paid_edges = emit_transfer_outputs(
        out_dir,
        transfers.all_txns,
        cfg.output.emit_raw_ledger,
    )

    tables: list[tuple[schema.CsvSpec, Iterable[emit.CsvRow]]] = [
        (schema.PERSON, emit.iter_person_rows(entities.people)),
        (schema.ACCOUNTNUMBER, emit.iter_account_rows(entities.accounts)),
        (schema.PHONE, emit.iter_phone_rows(entities.pii)),
        (schema.EMAIL, emit.iter_email_rows(entities.pii)),
        (schema.DEVICE, emit.iter_device_rows(infra_state.devices)),
        (schema.IPADDRESS, emit.iter_ip_rows(infra_state.ipdata)),
        (schema.HAS_ACCOUNT, emit.iter_has_account_rows(entities.accounts)),
        (
            schema.HAS_PHONE,
            emit.iter_has_phone_rows(entities.people.people, entities.pii),
        ),
        (
            schema.HAS_EMAIL,
            emit.iter_has_email_rows(entities.people.people, entities.pii),
        ),
        (schema.HAS_USED, emit.iter_has_used_rows(infra_state.devices)),
        (schema.HAS_IP, emit.iter_has_ip_rows(infra_state.ipdata)),
    ]

    for spec, rows in tables:
        emit.write_csv(out_dir / spec.filename, spec.header, rows)

    emit.write_csv(
        out_dir / "merchants.csv",
        ("merchant_id", "counterparty_acct", "category", "weight", "in_bank"),
        emit.iter_merchants_rows(entities.merchants),
    )
    emit.write_csv(
        out_dir / "external_accounts.csv",
        ("account_id", "kind", "category"),
        emit.iter_external_accounts_rows(entities.merchants),
    )
