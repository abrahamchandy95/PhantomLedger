from common import config
from common.random import Rng
from common.transactions import Transaction
from pipeline.state import Entities, Infra

from transfers.fraud import (
    Counterparties,
    InjectionInput,
    Parameters,
    Runtime,
    Scenario,
)
from transfers.legit.blueprints import (
    Blueprint,
    CCState,
    Overrides,
)
from transfers.legit.blueprints.models import Timeline, Network, Macro


def build_legit(
    cfg: config.World,
    rng: Rng,
    entities: Entities,
    infra: Infra,
) -> Blueprint:
    return Blueprint(
        timeline=Timeline(
            window=cfg.window,
            rng=rng,
        ),
        network=Network(
            accounts=entities.accounts,
            merchants=entities.merchants,
            portfolios=entities.portfolios,
        ),
        macro=Macro(
            pop=cfg.population,
            hubs=cfg.hubs,
            personas=cfg.personas,
            events=cfg.events,
            merchants_cfg=cfg.merchants,
            government=cfg.government,
        ),
        overrides=Overrides(
            infra=infra.router,
            persona_names=None,
            persona_for_person=entities.persona_map,
            persona_objects=entities.persona_objects,
            family_cfg=cfg.family,
            counterparty_pools=entities.counterparty_pools,
        ),
        cc_state=CCState(
            cards=entities.credit_cards,
        ),
    )


def build_fraud(
    cfg: config.World,
    rng: Rng,
    entities: Entities,
    infra: Infra,
    *,
    draft_txns: list[Transaction],
    biller_accounts: list[str],
    employers: list[str],
) -> InjectionInput:
    return InjectionInput(
        scenario=Scenario(
            fraud_cfg=cfg.fraud,
            window=cfg.window,
            people=entities.people,
            accounts=entities.accounts,
            base_txns=draft_txns,
        ),
        runtime=Runtime(
            rng=rng,
            infra=infra.router,
            ring_infra=infra.ring_infra,
        ),
        counterparties=Counterparties(
            biller_accounts=tuple(biller_accounts),
            employers=tuple(employers),
        ),
        params=Parameters(),
    )
