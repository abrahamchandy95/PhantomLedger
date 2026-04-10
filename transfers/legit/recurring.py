from datetime import timedelta

from common.channels import SALARY, RENT
from common.transactions import Transaction
from math_models.amount_model import (
    SALARY as SALARY_MODEL,
    RENT as RENT_MODEL,
)
import relationships.recurring as recurring_model
from transfers.factory import TransactionDraft, TransactionFactory

from .models import LegitInputs, LegitPolicies
from .plans import LegitBuildPlan


def _salary_people(
    inputs: LegitInputs,
    policies: LegitPolicies,
    plan: LegitBuildPlan,
) -> list[str]:
    salary_fraction = float(policies.recurring.salary_fraction)
    salary_candidates = [
        person_id
        for person_id in plan.persons
        if plan.primary_acct_for_person.get(person_id)
        not in plan.counterparties.hub_set
    ]

    salary_people_n = min(
        int(salary_fraction * len(salary_candidates)),
        len(salary_candidates),
    )
    if salary_people_n <= 0:
        return []

    return inputs.rng.choice_k(
        salary_candidates,
        salary_people_n,
        replace=False,
    )


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


def _rent_payers(
    inputs: LegitInputs,
    policies: LegitPolicies,
    plan: LegitBuildPlan,
) -> list[str]:
    rent_fraction = float(policies.recurring.rent_fraction)
    rent_candidates = [
        acct
        for acct in plan.primary_acct_for_person.values()
        if acct not in plan.counterparties.hub_set
    ]

    rent_n = min(int(rent_fraction * len(rent_candidates)), len(rent_candidates))
    if rent_n <= 0:
        return []

    return inputs.rng.choice_k(rent_candidates, rent_n, replace=False)


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
