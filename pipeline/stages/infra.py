from infra.devices import generate_devices
from infra.ipaddrs import generate_ipaddrs
from infra.txn_infra import TxnInfraAssigner
from pipeline.state import GenerationState, InfraStageData


def build_infra(st: GenerationState) -> None:
    cfg = st.cfg
    rng = st.rng

    entities = st.require_entities()

    devices = generate_devices(cfg.window, cfg.fraud, rng, entities.people)
    ipdata = generate_ipaddrs(cfg.window, cfg.fraud, rng, entities.people)

    infra = TxnInfraAssigner.build(
        cfg.infra,
        rng,
        acct_owner=entities.accounts.acct_owner,
        person_devices=devices.person_devices,
        person_ips=ipdata.person_ips,
    )

    st.infra = InfraStageData(
        devices=devices,
        ipdata=ipdata,
        infra=infra,
    )
