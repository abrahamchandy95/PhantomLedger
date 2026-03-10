import entities.credit_cards as credit_cards_entity
from entities.accounts import generate_accounts, with_extra_accounts
from entities.merchants import generate_merchants
from entities.people import generate_people
from entities.personas import assign_personas
from entities.pii import generate_pii
from pipeline.state import EntityStageData, GenerationState


def build_entities(st: GenerationState) -> None:
    cfg = st.cfg
    rng = st.rng

    people = generate_people(cfg.population, cfg.fraud, rng)
    accounts = generate_accounts(cfg.accounts, rng, people)
    pii = generate_pii(people.people, rng)

    merchants = generate_merchants(cfg.merchants, cfg.population, rng)
    if merchants.in_bank_accounts:
        accounts = with_extra_accounts(accounts, merchants.in_bank_accounts)

    persons = list(accounts.person_accounts.keys())
    persona_for_person = assign_personas(cfg.personas, rng, persons)

    accounts, credit_cards = credit_cards_entity.attach_credit_cards(
        credit_cards_entity.DEFAULT_CREDIT_ISSUANCE_POLICY,
        rng,
        credit_cards_entity.CreditCardIssuanceRequest(
            base_seed=cfg.population.seed,
            accounts=accounts,
            persona_for_person=persona_for_person,
        ),
    )

    st.entities = EntityStageData(
        people=people,
        accounts=accounts,
        pii=pii,
        merchants=merchants,
        persona_for_person=persona_for_person,
        credit_cards=credit_cards,
    )
