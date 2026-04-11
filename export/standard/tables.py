from collections.abc import Iterable

from common import schema
from pipeline.state import Entities, Infra

from ..csv_io import Row
from .accounts import (
    account_number as gen_account_number,
    has_account as gen_has_account,
)
from .external_accounts import external_account as gen_external_account
from .infra import (
    device as gen_device,
    has_ip as gen_has_ip,
    has_used as gen_has_used,
    ip_address as gen_ip_address,
)
from .merchants import merchant as gen_merchant
from .people import person as gen_person
from .pii import (
    email as gen_email,
    has_email as gen_has_email,
    has_phone as gen_has_phone,
    phone as gen_phone,
)

type OutputTable = tuple[schema.Table, Iterable[Row]]


def build(entities: Entities, infra: Infra) -> list[OutputTable]:
    """Maps target schemas to their respective data generators."""
    return [
        (schema.PERSON, gen_person(entities.people)),
        (schema.ACCOUNT_NUMBER, gen_account_number(entities.accounts)),
        (schema.PHONE, gen_phone(entities.pii)),
        (schema.EMAIL, gen_email(entities.pii)),
        (schema.DEVICE, gen_device(infra.devices)),
        (schema.IP_ADDRESS, gen_ip_address(infra.ips)),
        (schema.MERCHANT, gen_merchant(entities.merchants)),
        (
            schema.EXTERNAL_ACCOUNT,
            gen_external_account(entities.accounts, entities.merchants),
        ),
        (schema.HAS_ACCOUNT, gen_has_account(entities.accounts)),
        (schema.HAS_PHONE, gen_has_phone(entities.people.ids, entities.pii)),
        (schema.HAS_EMAIL, gen_has_email(entities.people.ids, entities.pii)),
        (schema.HAS_USED, gen_has_used(infra.devices)),
        (schema.HAS_IP, gen_has_ip(infra.ips)),
    ]
