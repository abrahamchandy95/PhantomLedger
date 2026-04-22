"""
AML exporter for TigerGraph AML_Schema_V1.
"""

from __future__ import annotations

from collections.abc import Iterator
from datetime import datetime
from pathlib import Path
from typing import cast

from common.progress import maybe_tqdm, status
from common.run import UseCase
from common.schema import LEDGER
from export.csv_io import Cell, write as write_csv
from export.registry import register
from export.standard.transfers import ledger
from pipeline.result import SimulationResult

import export.aml.edges as edges
import export.aml.schema as schema
import export.aml.vertices as vertices
from export.aml.sar import generate_sars

type Row = tuple[Cell, ...]


def _empty_rows() -> Iterator[Row]:
    return cast(Iterator[Row], iter(()))


@register(UseCase.AML)
def export(
    result: SimulationResult,
    out_dir: Path,
    show_transactions: bool,
    include_standard_export: bool = True,
) -> None:
    _ = include_standard_export

    e = result.entities
    i = result.infra
    t = result.transfers

    if t.final_txns:
        sim_start_dt: datetime = min(txn.timestamp for txn in t.final_txns)
    else:
        sim_start_dt = datetime(2025, 1, 1)

    status("AML Export: generating SARs...")
    sars = generate_sars(e.people, e.accounts, t.final_txns)

    status("AML Export: classifying transaction edges...")
    (
        send_rows,
        receive_rows,
        cp_send_rows,
        cp_receive_rows,
        sent_to_cp_pairs,
        received_from_cp_pairs,
        cp_senders,
        cp_receivers,
    ) = edges.transaction_edge_rows(e, t)

    status("AML Export: computing MinHash signatures...")
    nm_mh, addr_mh, street_mh, city_mh, state_mh = edges.minhash_vertex_sets(e)

    vtx_dir = out_dir / "aml" / "vertices"
    vtx_dir.mkdir(parents=True, exist_ok=True)

    vertex_tables = [
        (schema.CUSTOMER, vertices.customer_rows(e, sim_start_dt)),
        (schema.ACCOUNT, vertices.account_rows(e, t, sim_start_dt)),
        (schema.COUNTERPARTY, vertices.counterparty_rows(e)),
        (schema.NAME, vertices.name_rows(e)),
        (schema.ADDRESS, vertices.address_rows(e)),
        (schema.COUNTRY, vertices.country_rows()),
        (schema.DEVICE, vertices.device_rows(i)),
        (schema.TRANSACTION, vertices.transaction_rows(t)),
        (schema.SAR, vertices.sar_rows(sars)),
        (schema.BANK, vertices.bank_rows(e)),
        (schema.WATCHLIST, vertices.watchlist_rows(e, sim_start_dt)),
        (schema.NAME_MINHASH, ((mh_id,) for mh_id in sorted(nm_mh))),
        (schema.ADDRESS_MINHASH, ((mh_id,) for mh_id in sorted(addr_mh))),
        (schema.STREET_LINE1_MINHASH, ((mh_id,) for mh_id in sorted(street_mh))),
        (schema.CITY_MINHASH, ((mh_id,) for mh_id in sorted(city_mh))),
        (schema.STATE_MINHASH, ((mh_id,) for mh_id in sorted(state_mh))),
        (schema.CONNECTED_COMPONENT, _empty_rows()),
    ]

    status("AML Export: writing vertex CSVs...")
    for spec, rows in maybe_tqdm(
        vertex_tables,
        desc="aml vertices",
        unit="table",
        leave=False,
    ):
        write_csv(vtx_dir / spec.filename, spec.header, rows)

    edge_dir = out_dir / "aml" / "edges"
    edge_dir.mkdir(parents=True, exist_ok=True)

    edge_tables = [
        (schema.CUSTOMER_HAS_ACCOUNT, edges.customer_has_account_rows(e)),
        (
            schema.ACCOUNT_HAS_PRIMARY_CUSTOMER,
            edges.account_has_primary_customer_rows(e),
        ),
        (schema.SEND_TRANSACTION, iter(send_rows)),
        (schema.RECEIVE_TRANSACTION, iter(receive_rows)),
        (schema.COUNTERPARTY_SEND_TRANSACTION, iter(cp_send_rows)),
        (schema.COUNTERPARTY_RECEIVE_TRANSACTION, iter(cp_receive_rows)),
        (
            schema.SENT_TRANSACTION_TO_COUNTERPARTY,
            edges.sent_transaction_to_counterparty_rows(sent_to_cp_pairs),
        ),
        (
            schema.RECEIVED_TRANSACTION_FROM_COUNTERPARTY,
            edges.received_transaction_from_counterparty_rows(received_from_cp_pairs),
        ),
        (schema.USES_DEVICE, edges.uses_device_rows(i)),
        (schema.LOGGED_FROM, edges.logged_from_rows(e, i)),
        (schema.CUSTOMER_HAS_NAME, edges.customer_has_name_rows(e, sim_start_dt)),
        (schema.CUSTOMER_HAS_ADDRESS, edges.customer_has_address_rows(e, sim_start_dt)),
        (
            schema.CUSTOMER_ASSOCIATED_WITH_COUNTRY,
            edges.customer_associated_with_country_rows(e, sim_start_dt),
        ),
        (schema.ACCOUNT_HAS_NAME, edges.account_has_name_rows(e, sim_start_dt)),
        (schema.ACCOUNT_HAS_ADDRESS, edges.account_has_address_rows(e, sim_start_dt)),
        (
            schema.ACCOUNT_ASSOCIATED_WITH_COUNTRY,
            edges.account_associated_with_country_rows(e, sim_start_dt),
        ),
        (schema.ADDRESS_IN_COUNTRY, edges.address_in_country_rows(e, sim_start_dt)),
        (
            schema.COUNTERPARTY_HAS_NAME,
            edges.counterparty_has_name_rows(e, sim_start_dt),
        ),
        (
            schema.COUNTERPARTY_HAS_ADDRESS,
            edges.counterparty_has_address_rows(e, sim_start_dt),
        ),
        (
            schema.COUNTERPARTY_ASSOCIATED_WITH_COUNTRY,
            edges.counterparty_associated_with_country_rows(e),
        ),
        (
            schema.CUSTOMER_MATCHES_WATCHLIST,
            edges.customer_matches_watchlist_rows(e),
        ),
        (schema.REFERENCES, edges.references_rows(sars)),
        (schema.SAR_COVERS, edges.sar_covers_rows(sars)),
        (schema.BENEFICIARY_BANK, edges.beneficiary_bank_rows(cp_receivers)),
        (schema.ORIGINATOR_BANK, edges.originator_bank_rows(cp_senders)),
        (
            schema.BANK_ASSOCIATED_WITH_COUNTRY,
            edges.bank_associated_with_country_rows(e),
        ),
        (schema.BANK_HAS_ADDRESS, edges.bank_has_address_rows(e, sim_start_dt)),
        (schema.BANK_HAS_NAME, edges.bank_has_name_rows(e, sim_start_dt)),
        (
            schema.CUSTOMER_HAS_NAME_MINHASH,
            edges.customer_has_name_minhash_rows(e),
        ),
        (
            schema.CUSTOMER_HAS_ADDRESS_MINHASH,
            edges.customer_has_address_minhash_rows(e),
        ),
        (
            schema.CUSTOMER_HAS_ADDRESS_STREET_LINE1_MINHASH,
            edges.customer_has_address_street_line1_minhash_rows(e),
        ),
        (
            schema.CUSTOMER_HAS_ADDRESS_CITY_MINHASH,
            edges.customer_has_address_city_minhash_rows(e),
        ),
        (
            schema.CUSTOMER_HAS_ADDRESS_STATE_MINHASH,
            edges.customer_has_address_state_minhash_rows(e),
        ),
        (
            schema.ACCOUNT_HAS_NAME_MINHASH,
            edges.account_has_name_minhash_rows(e),
        ),
        (
            schema.COUNTERPARTY_HAS_NAME_MINHASH,
            edges.counterparty_has_name_minhash_rows(e),
        ),
        (schema.RESOLVES_TO, edges.resolves_to_rows(e, sim_start_dt)),
        (schema.SAME_AS, _empty_rows()),
        (schema.CUSTOMER_IN_CONNECTED_COMPONENT, _empty_rows()),
    ]

    status("AML Export: writing edge CSVs...")
    for spec, rows in maybe_tqdm(
        edge_tables,
        desc="aml edges",
        unit="table",
        leave=False,
    ):
        write_csv(edge_dir / spec.filename, spec.header, rows)

    if show_transactions:
        write_csv(
            out_dir / LEDGER.filename,
            LEDGER.header,
            ledger(t.final_txns),
        )

    total = len(t.final_txns)
    illicit = sum(1 for txn in t.final_txns if txn.fraud_flag == 1)
    ratio = illicit / max(1, total)
    n_internal = sum(1 for a in e.accounts.ids if a not in e.accounts.externals)

    print(f"AML Export complete → {out_dir}/aml/")
    print(f"  Customers:       {len(e.people.ids)}")
    print(f"  Accounts:        {n_internal}")
    print(f"  Counterparties:  {len(e.accounts.externals)}")
    print(f"  Transactions:    {total}  (illicit: {illicit}, {ratio:.4%})")
    print(f"  Fraud rings:     {len(e.people.rings)}")
    print(f"  Solo fraudsters: {len(e.people.solo_frauds)}")
    print(f"  SARs filed:      {len(sars)}")
