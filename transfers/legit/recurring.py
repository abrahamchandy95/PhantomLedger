from datetime import timedelta

from common.channels import SALARY, RENT
from common.persona_names import (
    FREELANCER,
    HNW,
    RETIRED,
    SALARIED,
    SMALLBIZ,
    STUDENT,
)
from common.transactions import Transaction
from math_models.amount_model import (
    SALARY as SALARY_MODEL,
    RENT as RENT_MODEL,
)
import relationships.recurring as recurring_model
from transfers.factory import TransactionDraft, TransactionFactory

from .models import LegitInputs, LegitPolicies
from .plans import LegitBuildPlan


def _scaled_probability(
    *,
    base_p: float,
    policy_fraction: float,
    baseline_fraction: float,
) -> float:
    """
    Preserve the existing policy knob while switching from
    random-global assignment to persona-aware assignment.

    Example:
      if baseline_fraction is the old global default (0.65 for salary,
      0.55 for rent), then changing policy_fraction still scales the
      persona-specific probabilities up or down.
    """
    if base_p <= 0.0 or policy_fraction <= 0.0 or baseline_fraction <= 0.0:
        return 0.0

    scaled = base_p * (policy_fraction / baseline_fraction)
    return max(0.0, min(1.0, scaled))


def _salary_probability_for_persona(persona: str) -> float:
    """
    Probability of receiving regular payroll-like deposits.

    This is deliberately broader than strict W-2 employment because the
    current model does not have a separate recurring business-revenue
    generator for freelancers/small businesses.
    """
    table = {
        SALARIED: 0.98,
        FREELANCER: 0.50,
        SMALLBIZ: 0.55,
        HNW: 0.30,
        STUDENT: 0.18,
        RETIRED: 0.04,
    }
    return float(table.get(persona, table[SALARIED]))


def _rent_probability_for_persona(persona: str) -> float:
    """
    Conditional probability of being a renter, *given that the person
    does not already hold a mortgage*.
    """
    table = {
        STUDENT: 0.50,
        RETIRED: 0.18,
        SALARIED: 0.62,
        FREELANCER: 0.58,
        SMALLBIZ: 0.35,
        HNW: 0.10,
    }
    return float(table.get(persona, table[SALARIED]))


def _salary_people(
    inputs: LegitInputs,
    policies: LegitPolicies,
    plan: LegitBuildPlan,
) -> list[str]:
    salary_fraction = float(policies.recurring.salary_fraction)

    out: list[str] = []
    for person_id in plan.persons:
        acct = plan.primary_acct_for_person.get(person_id)
        if acct is None:
            continue
        if acct in plan.counterparties.hub_set:
            continue

        persona = plan.personas.persona_for_person.get(person_id, SALARIED)
        p = _scaled_probability(
            base_p=_salary_probability_for_persona(persona),
            policy_fraction=salary_fraction,
            baseline_fraction=0.65,
        )
        if inputs.rng.coin(p):
            out.append(person_id)

    return out


def generate_salary_txns(
    inputs: LegitInputs,
    policies: LegitPolicies,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
) -> list[Transaction]:
    recurring_policy = policies.recurring
    rng = inputs.rng
    salary_people = _salary_people(inputs, policies, plan)

    employment: dict[str, recurring_model.Employment] = {}
    txns: list[Transaction] = []

    def base_salary_draw() -> float:
        return SALARY_MODEL.sample(rng)

    for person_id in salary_people:
        employment[person_id] = recurring_model.employment.initialize(
            recurring_policy,
            plan.seed,
            person_id=person_id,
            start_date=plan.start_date,
            employers=plan.counterparties.employers,
            base_salary_source=base_salary_draw,
        )

    for payday in plan.paydays:
        for person_id in salary_people:
            state = employment[person_id]
            while payday >= state.end:
                state = recurring_model.employment.advance(
                    recurring_policy,
                    plan.seed,
                    person_id=person_id,
                    now=state.end,
                    employers=plan.counterparties.employers,
                    prev=state,
                )
            employment[person_id] = state

            dst_acct = plan.primary_acct_for_person.get(person_id)
            if dst_acct is None:
                continue

            txn_ts = payday + timedelta(
                hours=rng.int(8, 18),
                minutes=rng.int(0, 60),
            )
            amount = recurring_model.employment.calculate_salary(
                recurring_policy,
                plan.seed,
                person_id=person_id,
                state=state,
                pay_date=payday,
            )

            txns.append(
                txf.make(
                    TransactionDraft(
                        source=state.employer_acct,
                        destination=dst_acct,
                        amount=amount,
                        timestamp=txn_ts,
                        channel=SALARY,
                        is_fraud=0,
                        ring_id=-1,
                    )
                )
            )

    return txns


def _mortgage_holders(inputs: LegitInputs) -> set[str]:
    portfolios = inputs.portfolios
    if portfolios is None:
        return set()

    return {
        person_id
        for person_id, portfolio in portfolios.by_person.items()
        if portfolio.mortgage is not None
    }


def _rent_payers(
    inputs: LegitInputs,
    policies: LegitPolicies,
    plan: LegitBuildPlan,
) -> list[str]:
    rent_fraction = float(policies.recurring.rent_fraction)
    mortgage_holders = _mortgage_holders(inputs)

    out: list[str] = []
    for person_id, acct in plan.primary_acct_for_person.items():
        if acct in plan.counterparties.hub_set:
            continue
        if person_id in mortgage_holders:
            continue

        persona = plan.personas.persona_for_person.get(person_id, SALARIED)
        p = _scaled_probability(
            base_p=_rent_probability_for_persona(persona),
            policy_fraction=rent_fraction,
            baseline_fraction=0.55,
        )
        if inputs.rng.coin(p):
            out.append(acct)

    return out


def generate_rent_txns(
    inputs: LegitInputs,
    policies: LegitPolicies,
    plan: LegitBuildPlan,
    txf: TransactionFactory,
) -> list[Transaction]:
    recurring_policy = policies.recurring
    rng = inputs.rng
    rent_active = _rent_payers(inputs, policies, plan)

    leases: dict[str, recurring_model.Lease] = {}
    txns: list[Transaction] = []

    def base_rent_draw() -> float:
        return RENT_MODEL.sample(rng)

    for payer_acct in rent_active:
        leases[payer_acct] = recurring_model.lease.initialize(
            recurring_policy,
            plan.seed,
            rng,
            payer_acct=payer_acct,
            start_date=plan.start_date,
            landlords=plan.counterparties.landlords,
            base_rent_source=base_rent_draw,
        )

    for payday in plan.paydays:
        for payer_acct in rent_active:
            state = leases[payer_acct]
            while payday >= state.end:
                state = recurring_model.lease.advance(
                    recurring_policy,
                    plan.seed,
                    rng,
                    payer_acct=payer_acct,
                    now=state.end,
                    landlords=plan.counterparties.landlords,
                    prev=state,
                    reset_rent_source=base_rent_draw,
                )
            leases[payer_acct] = state

            txn_ts = payday + timedelta(
                days=rng.int(0, 5),
                hours=rng.int(7, 22),
                minutes=rng.int(0, 60),
            )
            amount = recurring_model.lease.calculate_rent(
                recurring_policy,
                plan.seed,
                payer_acct=payer_acct,
                state=state,
                pay_date=payday,
            )

            txns.append(
                txf.make(
                    TransactionDraft(
                        source=payer_acct,
                        destination=state.landlord_acct,
                        amount=amount,
                        timestamp=txn_ts,
                        channel=RENT,
                        is_fraud=0,
                        ring_id=-1,
                    )
                )
            )

    return txns
