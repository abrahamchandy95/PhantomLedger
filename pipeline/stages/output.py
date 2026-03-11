import emit
from pipeline.state import GenerationState
from transfers.aggregate import emit_transfer_outputs

from .output_tables import build_output_tables


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

    for spec, rows in build_output_tables(entities, infra_state):
        emit.write_csv(out_dir / spec.filename, spec.header, rows)
