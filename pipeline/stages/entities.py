"""
Entity assembly stage.

Builds and assembles the full entity universe for a simulation world:
people, accounts, PII, merchants, landlords, counterparties, personas,
owned/external family accounts, credit cards, and portfolios.

Key invariant:
- internal counterparties are merged without `mark_external`
- external counterparties are merged with `mark_external=True`
"""

from common import config
from common.business_accounts import planned_owned_income_accounts
from common.externals import ALL as ALL_EXTERNALS
from common.family_accounts import planned_external_family_accounts
from common.random import Rng
from pipeline.state import Entities

from entities.accounts import (
    build as build_accounts,
    merge,
    merge_owned_accounts,
)
from entities.counterparties import build_pools
from entities.credit_cards import DEFAULT_POLICY, build as build_credit_cards
from entities.landlords import build as build_landlords
from entities.merchants import build as build_merchants
from entities.people import generate as generate_people
from entities.personas import assign as assign_personas, build_persona_objects
from entities.pii import generate as generate_pii
from entities.products import build_portfolios


def build(cfg: config.World, rng: Rng) -> Entities:
    """
    Build the complete entity universe for a simulation world.
    """
    people = generate_people(cfg.population, cfg.fraud, rng)
    accounts = build_accounts(cfg.accounts, rng, people)
    pii = generate_pii(people, rng)

    # Merchants
    merchants = build_merchants(cfg.merchants, cfg.population, rng)
    if merchants.internals:
        accounts = merge(accounts, merchants.internals)
    if merchants.externals:
        accounts = merge(accounts, merchants.externals, mark_external=True)

    # Landlords
    landlords = build_landlords(cfg.landlords, cfg.population, rng)
    if landlords.internals:
        accounts = merge(accounts, landlords.internals)
    if landlords.externals:
        accounts = merge(accounts, landlords.externals, mark_external=True)

    # Counterparty pools
    counterparty_pools = build_pools(cfg.counterparties, cfg.population, rng)
    if counterparty_pools.all_internals:
        accounts = merge(accounts, counterparty_pools.all_internals)
    if counterparty_pools.all_externals:
        accounts = merge(
            accounts,
            counterparty_pools.all_externals,
            mark_external=True,
        )

    # External institutional accounts
    accounts = merge(
        accounts,
        list(ALL_EXTERNALS),
        mark_external=True,
    )

    # Primary personal account per known person
    primary_accounts = {
        person_id: account_ids[0]
        for person_id, account_ids in accounts.by_person.items()
        if account_ids
    }

    # Deterministic family external accounts for known people
    family_external_accounts = planned_external_family_accounts(
        people.ids,
        primary_accounts,
        float(cfg.family_routing.external_p),
    )
    if family_external_accounts:
        accounts = merge_owned_accounts(
            accounts,
            family_external_accounts,
            mark_external=True,
        )

    # Personas
    persons = list(accounts.by_person.keys())
    persona_map = assign_personas(cfg.personas, rng, persons)
    persona_objects = build_persona_objects(persona_map, cfg.population.seed)

    # Same-bank owned business / brokerage accounts
    owned_income_accounts = planned_owned_income_accounts(
        person_ids=people.ids,
        persona_for_person=persona_map,
        primary_accounts=primary_accounts,
    )
    if owned_income_accounts:
        accounts = merge_owned_accounts(
            accounts,
            owned_income_accounts,
            mark_external=False,
        )

    # Credit cards
    #
    # Refunds and chargebacks on these cards flow back from the actual
    # merchant that originally received the purchase, not from a synthetic
    # per-card "refund source" counterparty. See
    # transfers/credit_cards/statement.py::sample_merchant_credit.
    accounts, credit_cards = build_credit_cards(
        DEFAULT_POLICY,
        cfg.population.seed,
        accounts,
        persona_objects,
    )

    # Financial product portfolios
    portfolios = build_portfolios(
        base_seed=cfg.population.seed,
        persona_map=persona_map,
        credit_cards=credit_cards,
        insurance_cfg=cfg.insurance,
        start_date=cfg.window.start_date,
    )

    return Entities(
        people=people,
        accounts=accounts,
        pii=pii,
        merchants=merchants,
        landlords=landlords,
        persona_map=persona_map,
        persona_objects=persona_objects,
        credit_cards=credit_cards,
        counterparty_pools=counterparty_pools,
        portfolios=portfolios,
    )
