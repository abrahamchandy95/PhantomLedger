from dataclasses import dataclass
from datetime import datetime

from common.temporal import iter_window_month_starts
from entities.accounts import AccountsData
from entities.personas import assign_personas

from .models import LegitInputs, LegitOverrides


DEFAULT_PERSONA_NAMES: tuple[str, ...] = (
    "student",
    "retired",
    "freelancer",
    "smallbiz",
    "hnw",
    "salaried",
)


@dataclass(frozen=True, slots=True)
class CounterpartyPlan:
    hub_accounts: list[str]
    hub_set: set[str]
    biller_accounts: list[str]
    employers: list[str]
    issuer_acct: str


@dataclass(frozen=True, slots=True)
class PersonaPlan:
    persona_for_person: dict[str, str]
    persona_names: list[str]


@dataclass(frozen=True, slots=True)
class LegitBuildPlan:
    start_date: datetime
    days: int
    seed: int
    all_accounts: list[str]
    persons: list[str]
    paydays: list[datetime]
    primary_acct_for_person: dict[str, str]
    counterparties: CounterpartyPlan
    personas: PersonaPlan


def _select_hub_accounts(
    inputs: LegitInputs,
) -> list[str]:
    persons = list(inputs.accounts.person_accounts.keys())
    if not persons:
        return []

    n_hubs = int(inputs.pop.persons * inputs.hubs.hub_fraction)
    n_hubs = max(1, min(n_hubs, len(persons)))

    hub_people = inputs.rng.choice_k(persons, n_hubs, replace=False)
    return [inputs.accounts.person_accounts[person_id][0] for person_id in hub_people]


def _primary_acct_for_person(accounts: AccountsData) -> dict[str, str]:
    return {
        person_id: acct_ids[0]
        for person_id, acct_ids in accounts.person_accounts.items()
        if acct_ids
    }


def _build_counterparty_plan(inputs: LegitInputs) -> CounterpartyPlan:
    all_accounts = inputs.accounts.accounts
    hub_accounts = _select_hub_accounts(inputs)
    hub_set = set(hub_accounts)

    biller_accounts = hub_accounts if hub_accounts else [all_accounts[0]]
    employers = (
        hub_accounts[: max(1, len(hub_accounts) // 5)]
        if hub_accounts
        else [all_accounts[0]]
    )
    issuer_acct = hub_accounts[0] if hub_accounts else all_accounts[0]

    return CounterpartyPlan(
        hub_accounts=hub_accounts,
        hub_set=hub_set,
        biller_accounts=biller_accounts,
        employers=employers,
        issuer_acct=issuer_acct,
    )


def _build_persona_plan(
    inputs: LegitInputs,
    overrides: LegitOverrides,
    persons: list[str],
) -> PersonaPlan:
    persona_for_person = overrides.persona_for_person
    if persona_for_person is None:
        persona_for_person = assign_personas(
            inputs.personas,
            inputs.rng,
            persons,
        )

    persona_names = overrides.persona_names
    if persona_names is None:
        persona_names = list(DEFAULT_PERSONA_NAMES)

    return PersonaPlan(
        persona_for_person=persona_for_person,
        persona_names=persona_names,
    )


def build_legit_plan(
    inputs: LegitInputs,
    overrides: LegitOverrides,
) -> LegitBuildPlan:
    start_date = inputs.window.start_date()
    days = int(inputs.window.days)
    seed = int(inputs.pop.seed)
    persons = list(inputs.accounts.person_accounts.keys())

    counterparties = _build_counterparty_plan(inputs)
    personas = _build_persona_plan(inputs, overrides, persons)
    primary_acct_for_person = _primary_acct_for_person(inputs.accounts)
    paydays = iter_window_month_starts(start_date, days)

    return LegitBuildPlan(
        start_date=start_date,
        days=days,
        seed=seed,
        all_accounts=inputs.accounts.accounts,
        persons=persons,
        paydays=paydays,
        primary_acct_for_person=primary_acct_for_person,
        counterparties=counterparties,
        personas=personas,
    )
