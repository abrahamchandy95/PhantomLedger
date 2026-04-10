from dataclasses import dataclass

import entities.models as models


@dataclass(frozen=True, slots=True)
class Plan:
    """Maps the human members of a fraud ring to their primary bank accounts."""

    ring_id: int
    fraud_accounts: tuple[str, ...]
    mule_accounts: tuple[str, ...]
    victim_accounts: tuple[str, ...]

    @property
    def participant_accounts(self) -> tuple[str, ...]:
        """All complicit accounts in the ring (organizers + mules)."""
        return self.fraud_accounts + self.mule_accounts


def _get_primary(accounts: models.Accounts, person_id: str) -> str:
    """Extracts the primary routing account for a given person."""
    accts = accounts.by_person.get(person_id)
    if not accts:
        raise ValueError(f"Person has no active accounts: {person_id}")
    return accts[0]


def build_plan(ring: models.Ring, accounts: models.Accounts) -> Plan:
    """Translates a logical Ring of people into a concrete Plan of accounts."""
    return Plan(
        ring_id=ring.id,
        fraud_accounts=tuple(_get_primary(accounts, p) for p in ring.frauds),
        mule_accounts=tuple(_get_primary(accounts, p) for p in ring.mules),
        victim_accounts=tuple(_get_primary(accounts, p) for p in ring.victims),
    )
