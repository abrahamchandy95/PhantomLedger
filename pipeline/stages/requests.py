import entities.credit_cards as credit_cards_entity
from common.transactions import Transaction
from pipeline.state import EntityStageData, GenerationState
import transfers.fraud as fraud_model
import transfers.legit as legit_model


def _replace_credit_cards(
    entities: EntityStageData,
    credit_cards: credit_cards_entity.CreditCardData,
) -> EntityStageData:
    return EntityStageData(
        people=entities.people,
        accounts=entities.accounts,
        pii=entities.pii,
        merchants=entities.merchants,
        persona_for_person=entities.persona_for_person,
        credit_cards=credit_cards,
    )


def ensure_credit_cards_for_legit(
    st: GenerationState,
) -> credit_cards_entity.CreditCardData:
    """
    Return stage credit-card data, inserting an explicit empty value into
    state when the entity stage has not populated cards.
    """
    entities = st.require_entities()
    credit_cards = entities.credit_cards

    if credit_cards.card_accounts or credit_cards.card_for_person:
        return credit_cards

    empty_cards = credit_cards_entity.empty_credit_cards()
    st.entities = _replace_credit_cards(entities, empty_cards)
    return empty_cards


def build_legit_request(st: GenerationState) -> legit_model.LegitGenerationRequest:
    cfg = st.cfg
    entities = st.require_entities()
    infra_state = st.infra

    return legit_model.LegitGenerationRequest(
        inputs=legit_model.LegitInputs(
            window=cfg.window,
            pop=cfg.population,
            hubs=cfg.hubs,
            personas=cfg.personas,
            events=cfg.events,
            merchants_cfg=cfg.merchants,
            rng=st.rng,
            accounts=entities.accounts,
            merchants=entities.merchants,
        ),
        overrides=legit_model.LegitOverrides(
            infra=None if infra_state is None else infra_state.infra,
            persona_for_person=entities.persona_for_person,
            family_cfg=cfg.family,
        ),
        credit_runtime=legit_model.LegitCreditRuntime(
            cards=ensure_credit_cards_for_legit(st),
        ),
    )


def build_fraud_request(
    st: GenerationState,
    *,
    base_txns: list[Transaction],
    biller_accounts: list[str],
    employers: list[str],
) -> fraud_model.FraudInjectionRequest:
    entities = st.require_entities()
    infra_state = st.infra

    return fraud_model.FraudInjectionRequest(
        scenario=fraud_model.FraudScenario(
            fraud_cfg=st.cfg.fraud,
            window=st.cfg.window,
            people=entities.people,
            accounts=entities.accounts,
            base_txns=base_txns,
        ),
        runtime=fraud_model.FraudRuntime(
            rng=st.rng,
            infra=None if infra_state is None else infra_state.infra,
        ),
        counterparties=fraud_model.FraudCounterparties(
            biller_accounts=tuple(biller_accounts),
            employers=tuple(employers),
        ),
        policies=fraud_model.FraudPolicies(),
    )
