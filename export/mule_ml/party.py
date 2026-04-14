from collections.abc import Iterator

from common.schema import ML_PARTY
from pipeline.state import Entities, Infra, Transfers

from ..csv_io import Row
from .canonical import canonical_maps
from .identity import blank_identity, fake_identity, reference_year


HEADER = ML_PARTY.header


def all_party_ids(entities: Entities, transfers: Transfers) -> set[str]:
    ids = set(entities.accounts.ids)
    for txn in transfers.final_txns:
        ids.add(txn.source)
        ids.add(txn.target)
    return ids


def rows(
    entities: Entities,
    infra: Infra,
    transfers: Transfers,
) -> Iterator[Row]:
    party_ids = all_party_ids(entities, transfers)
    acct_to_device, acct_to_ip = canonical_maps(entities, infra, transfers, party_ids)
    ref_year = reference_year(transfers)

    mules = entities.accounts.mules
    orgs = entities.accounts.frauds
    solos = entities.people.solo_frauds

    for account_id in sorted(party_ids):
        owner = entities.accounts.owner_map.get(account_id)
        is_internal = owner is not None

        if is_internal:
            identity = fake_identity(
                owner,
                persona=entities.persona_map.get(owner),
                reference_year=ref_year,
            )
        else:
            identity = blank_identity()

        phone = entities.pii.phone_map.get(owner, "") if is_internal else ""
        email = entities.pii.email_map.get(owner, "") if is_internal else ""

        is_mule = int(account_id in mules)
        is_organizer = int(account_id in orgs)
        is_solo = int(is_internal and owner in solos)
        is_fraud = int(bool(is_mule or is_organizer or is_solo))

        yield (
            account_id,
            is_fraud,
            phone,
            email,
            identity["name"],
            identity["SSN"],
            identity["dob"],
            identity["address"],
            identity["state"],
            identity["city"],
            identity["zipcode"],
            identity["country"],
            acct_to_ip[account_id],
            acct_to_device[account_id],
        )
