from common import config
from common.externals import ALL as ALL_EXTERNALS
from common.family_accounts import planned_external_family_accounts
from common.random import Rng
from pipeline.state import Entities

from entities.accounts import (
    build as build_accounts,
    merge,
    merge_owned_accounts,
)
from entities.counterparties import build as build_counterparties
from entities.credit_cards import build as build_credit_cards, DEFAULT_POLICY
from entities.merchants import build as build_merchants
from entities.people import generate as generate_people
from entities.personas import assign as assign_personas, build_persona_objects
from entities.pii import generate as generate_pii
from entities.products import build_portfolios


def build(cfg: config.World, rng: Rng) -> Entities:
    people = generate_people(cfg.population, cfg.fraud, rng)
    accounts = build_accounts(cfg.accounts, rng, people)
    pii = generate_pii(people, rng)

    # Merchants & Shared Accounts
    merchants = build_merchants(cfg.merchants, cfg.population, rng)
    if merchants.internals:
        accounts = merge(accounts, merchants.internals)
    if merchants.externals:
        accounts = merge(accounts, merchants.externals, mark_external=True)

    # Employer & Landlord counterparties
    counterparty_pools = build_counterparties(cfg.population.size)
    if counterparty_pools.all_externals:
        accounts = merge(accounts, counterparty_pools.all_externals, mark_external=True)

    # External institutional accounts (gov, insurance, lenders, IRS)
    accounts = merge(
        accounts,
        list(ALL_EXTERNALS),
        mark_external=True,
    )

    # Deterministic family external accounts (XF...) for known people.
    # These accounts belong to a known synthetic person but are serviced by
    # another bank, so they must still be represented in the account registry.
    primary_accounts = {
        person_id: account_ids[0]
        for person_id, account_ids in accounts.by_person.items()
        if account_ids
    }
    family_external_accounts = planned_external_family_accounts(
        people.ids,
        primary_accounts,
        float(cfg.family.external_family_p),
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

    # Credit Cards (must happen before portfolio building)
    accounts, credit_cards = build_credit_cards(
        DEFAULT_POLICY,
        cfg.population.seed,
        accounts,
        persona_objects,
    )

    # Stable external refund-source accounts used by card refunds/chargebacks.
    # One per issued card keeps the account universe bounded and ensures
    # validate_transaction_accounts() sees these sources as registered.
    refund_external_accounts: list[str] = [
        f"XREFUND_{card_id}" for card_id in credit_cards.ids
    ]
    if refund_external_accounts:
        accounts = merge(
            accounts,
            refund_external_accounts,
            mark_external=True,
        )

    # Financial Product Portfolios
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
        persona_map=persona_map,
        persona_objects=persona_objects,
        credit_cards=credit_cards,
        counterparty_pools=counterparty_pools,
        portfolios=portfolios,
    )
