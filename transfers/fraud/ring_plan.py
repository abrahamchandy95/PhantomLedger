from dataclasses import dataclass

from entities.accounts import AccountsData
from entities.people import RingPeople


@dataclass(frozen=True, slots=True)
class RingPlan:
    ring_id: int
    fraud_accounts: tuple[str, ...]
    mule_accounts: tuple[str, ...]
    victim_accounts: tuple[str, ...]

    @property
    def participant_accounts(self) -> tuple[str, ...]:
        return self.fraud_accounts + self.mule_accounts


def _primary_account(accounts: AccountsData, person_id: str) -> str:
    accts = accounts.person_accounts.get(person_id)
    if not accts:
        raise ValueError(f"person has no accounts: {person_id}")
    return accts[0]


def build_ring_plan(ring: RingPeople, accounts: AccountsData) -> RingPlan:
    return RingPlan(
        ring_id=ring.ring_id,
        fraud_accounts=tuple(_primary_account(accounts, p) for p in ring.fraud_people),
        mule_accounts=tuple(_primary_account(accounts, p) for p in ring.mule_people),
        victim_accounts=tuple(
            _primary_account(accounts, p) for p in ring.victim_people
        ),
    )
