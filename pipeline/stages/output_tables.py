from collections.abc import Iterable

import common.schema as schema
import emit
from pipeline.state import EntityStageData, InfraStageData

type OutputTable = tuple[schema.CsvSpec, Iterable[emit.CsvRow]]


def build_output_tables(
    entities: EntityStageData,
    infra_state: InfraStageData,
) -> list[OutputTable]:
    return [
        (schema.PERSON, emit.iter_person_rows(entities.people)),
        (schema.ACCOUNT_NUMBER, emit.iter_account_rows(entities.accounts)),
        (schema.PHONE, emit.iter_phone_rows(entities.pii)),
        (schema.EMAIL, emit.iter_email_rows(entities.pii)),
        (schema.DEVICE, emit.iter_device_rows(infra_state.devices)),
        (schema.IP_ADDRESS, emit.iter_ip_rows(infra_state.ipdata)),
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
        (schema.MERCHANT, emit.iter_merchants_rows(entities.merchants)),
        (
            schema.EXTERNAL_ACCOUNT,
            emit.iter_external_accounts_rows(entities.merchants),
        ),
    ]
