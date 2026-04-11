from dataclasses import dataclass
from datetime import datetime

from common.timeline import active_months
import entities.models as models
import entities.personas as personas_mod

from .models import LegitInputs, LegitOverrides


@dataclass(frozen=True, slots=True)
class CounterpartyPlan:
    hub_accounts: list[str]
    hub_set: frozenset[str]
    employers: list[str]
    landlords: list[str]
    biller_accounts: list[str]
    issuer_acct: str


@dataclass(frozen=True, slots=True)
class PersonaPlan:
    persona_for_person: dict[str, str]
    persona_objects: dict[str, models.Persona]
    persona_names: list[str]


@dataclass(frozen=True, slots=True)
class LegitBuildPlan:
    start_date: datetime
    days: int
    seed: int
    all_accounts: list[str]
    persons: list[str]
    month_starts: list[datetime]
    primary_acct_for_person: dict[str, str]
    counterparties: CounterpartyPlan
    personas: PersonaPlan

    @property
    def paydays(self) -> list[datetime]:
        """
        Compatibility shim for older generators that still expect `plan.paydays`.
        These are month anchors, not true payroll schedules.
        """
        return self.month_starts


def _select_hub_accounts(
    inputs: LegitInputs,
) -> list[str]:
    persons = list(inputs.accounts.by_person)
    if not persons:
        return []

    n_hubs = int(inputs.pop.size * inputs.hubs.fraction)
    n_hubs = max(1, min(n_hubs, len(persons)))

    hub_people = inputs.rng.choice_k(persons, n_hubs, replace=False)
    return [inputs.accounts.by_person[person_id][0] for person_id in hub_people]


def _primary_acct_for_person(
    accounts: models.Accounts,
) -> dict[str, str]:
    return {
        person_id: acct_ids[0]
        for person_id, acct_ids in accounts.by_person.items()
        if acct_ids
    }


def _build_counterparty_plan(
    inputs: LegitInputs,
    overrides: LegitOverrides,
) -> CounterpartyPlan:
    all_accounts = inputs.accounts.ids
    if not all_accounts:
        raise ValueError("inputs.accounts.ids must be non-empty")

    hub_accounts = _select_hub_accounts(inputs)
    hub_set = frozenset(hub_accounts)

    fallback_acct = hub_accounts[0] if hub_accounts else all_accounts[0]

    pools = overrides.counterparty_pools

    if pools is not None and pools.employer_ids:
        employers = list(pools.employer_ids)
    else:
        employers = (
            hub_accounts[: max(1, len(hub_accounts) // 5)]
            if hub_accounts
            else [fallback_acct]
        )

    if pools is not None and pools.landlord_ids:
        landlords = list(pools.landlord_ids)
    else:
        landlords = hub_accounts if hub_accounts else [fallback_acct]

    biller_accounts = hub_accounts if hub_accounts else [fallback_acct]
    issuer_acct = fallback_acct

    return CounterpartyPlan(
        hub_accounts=hub_accounts,
        hub_set=hub_set,
        employers=employers,
        landlords=landlords,
        biller_accounts=biller_accounts,
        issuer_acct=issuer_acct,
    )


def _build_persona_plan(
    inputs: LegitInputs,
    overrides: LegitOverrides,
    persons: list[str],
) -> PersonaPlan:
    persona_for_person = overrides.persona_for_person
    if persona_for_person is None:
        persona_for_person = personas_mod.assign(
            inputs.personas,
            inputs.rng,
            persons,
        )

    persona_objects = overrides.persona_objects
    if persona_objects is None:
        persona_objects = {
            pid: personas_mod.get_persona(name)
            for pid, name in persona_for_person.items()
        }

    persona_names = overrides.persona_names
    if persona_names is None:
        persona_names = list(personas_mod.PERSONAS.keys())

    return PersonaPlan(
        persona_for_person=persona_for_person,
        persona_objects=persona_objects,
        persona_names=persona_names,
    )


def build_legit_plan(
    inputs: LegitInputs,
    overrides: LegitOverrides,
) -> LegitBuildPlan:
    start_date = inputs.window.start_date
    days = int(inputs.window.days)
    seed = int(inputs.pop.seed)
    persons = list(inputs.accounts.by_person)

    counterparties = _build_counterparty_plan(inputs, overrides)
    personas = _build_persona_plan(inputs, overrides, persons)
    primary_acct_for_person = _primary_acct_for_person(inputs.accounts)
    month_starts = active_months(start_date, days)

    return LegitBuildPlan(
        start_date=start_date,
        days=days,
        seed=seed,
        all_accounts=inputs.accounts.ids,
        persons=persons,
        month_starts=month_starts,
        primary_acct_for_person=primary_acct_for_person,
        counterparties=counterparties,
        personas=personas,
    )
