from dataclasses import dataclass
from datetime import datetime

from common.timeline import active_months
import entities.models as models
import entities.personas as personas_mod

from .models import Overrides, Timeline, Network, Macro


@dataclass(frozen=True, slots=True)
class CounterpartyPlan:
    hub_accounts: list[str]
    hub_set: frozenset[str]
    employers: list[str]

    # Flat ID list used by the lease engine. When the landlords pool is
    # empty (tiny populations, overridden config, etc.), this falls back
    # to hub accounts so the lease sampler still has something to pick.
    landlords: list[str]
    # acct_id -> landlord type (INDIVIDUAL / LLC_SMALL / CORPORATE). Missing
    # entries (e.g. fallback hub IDs) mean the rent router will use its
    # default channel.
    landlord_type_of: dict[str, str]

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
    timeline: Timeline,
    network: Network,
    macro: Macro,
) -> list[str]:
    persons = list(network.accounts.by_person)
    if not persons:
        return []

    n_hubs = int(macro.pop.size * macro.hubs.fraction)
    n_hubs = max(1, min(n_hubs, len(persons)))

    hub_people = timeline.rng.choice_k(persons, n_hubs, replace=False)
    return [network.accounts.by_person[person_id][0] for person_id in hub_people]


def _primary_acct_for_person(
    accounts: models.Accounts,
) -> dict[str, str]:
    return {
        person_id: acct_ids[0]
        for person_id, acct_ids in accounts.by_person.items()
        if acct_ids
    }


def _resolve_landlords(
    network: Network,
    hub_accounts: list[str],
    fallback_acct: str,
) -> tuple[list[str], dict[str, str]]:
    """
    Return (landlord_ids, type_of).

    Prefers the typed landlord pool built in entities/landlords.py. When that
    pool is empty the plan falls back to hub accounts (as the pre-typology
    code did) so the lease engine always has something to pick. Fallback
    IDs have no type mapping, which tells the rent router to use the
    default channel.
    """
    landlords = network.landlords
    if landlords.ids:
        return list(landlords.ids), dict(landlords.type_of)

    fallback = hub_accounts if hub_accounts else [fallback_acct]
    return fallback, {}


def _build_counterparty_plan(
    timeline: Timeline,
    network: Network,
    macro: Macro,
    overrides: Overrides,
) -> CounterpartyPlan:
    all_accounts = network.accounts.ids
    if not all_accounts:
        raise ValueError("network.accounts.ids must be non-empty")

    hub_accounts = _select_hub_accounts(timeline, network, macro)
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

    landlord_ids, landlord_type_of = _resolve_landlords(
        network, hub_accounts, fallback_acct
    )

    biller_accounts = hub_accounts if hub_accounts else [fallback_acct]
    issuer_acct = fallback_acct

    return CounterpartyPlan(
        hub_accounts=hub_accounts,
        hub_set=hub_set,
        employers=employers,
        landlords=landlord_ids,
        landlord_type_of=landlord_type_of,
        biller_accounts=biller_accounts,
        issuer_acct=issuer_acct,
    )


def _build_persona_plan(
    timeline: Timeline,
    macro: Macro,
    overrides: Overrides,
    persons: list[str],
) -> PersonaPlan:
    persona_for_person = overrides.persona_for_person
    if persona_for_person is None:
        persona_for_person = personas_mod.assign(
            macro.personas,
            timeline.rng,
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
    timeline: Timeline,
    network: Network,
    macro: Macro,
    overrides: Overrides,
) -> LegitBuildPlan:
    start_date = timeline.window.start_date
    days = int(timeline.window.days)
    seed = int(macro.pop.seed)
    persons = list(network.accounts.by_person)

    counterparties = _build_counterparty_plan(timeline, network, macro, overrides)
    personas = _build_persona_plan(timeline, macro, overrides, persons)
    primary_acct_for_person = _primary_acct_for_person(network.accounts)
    month_starts = active_months(start_date, days)

    return LegitBuildPlan(
        start_date=start_date,
        days=days,
        seed=seed,
        all_accounts=network.accounts.ids,
        persons=persons,
        month_starts=month_starts,
        primary_acct_for_person=primary_acct_for_person,
        counterparties=counterparties,
        personas=personas,
    )
