from datetime import timedelta

from common.transactions import Transaction
from math_models.amounts import bill_amount, salary_amount
import relationships.recurring as recurring_model
from transfers.txns import TxnFactory, TxnSpec

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
    txf: TxnFactory,
) -> list[Transaction]:
    recurring_policy = policies.recurring
    rng = inputs.rng
    salary_people = _salary_people(inputs, policies, plan)

    employment: dict[str, recurring_model.EmploymentState] = {}
    txns: list[Transaction] = []

    def base_salary_draw() -> float:
        return float(salary_amount(rng))

    for person_id in salary_people:
        employment[person_id] = recurring_model.init_employment(
            recurring_policy,
            plan.seed,
            rng,
            person_id=person_id,
            start_date=plan.start_date,
            employers=plan.counterparties.employers,
            base_salary_sampler=base_salary_draw,
        )

    for payday in plan.paydays:
        for person_id in salary_people:
            state = employment[person_id]
            while payday >= state.end:
                state = recurring_model.advance_employment(
                    recurring_policy,
                    plan.seed,
                    rng,
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
            amount = recurring_model.salary_at(
                recurring_policy,
                plan.seed,
                person_id=person_id,
                state=state,
                pay_date=payday,
            )

            txns.append(
                txf.make(
                    TxnSpec(
                        src=state.employer_acct,
                        dst=dst_acct,
                        amt=amount,
                        ts=txn_ts,
                        channel="salary",
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
    txf: TxnFactory,
) -> list[Transaction]:
    recurring_policy = policies.recurring
    rng = inputs.rng
    rent_active = _rent_payers(inputs, policies, plan)

    leases: dict[str, recurring_model.LeaseState] = {}
    txns: list[Transaction] = []

    def base_rent_draw() -> float:
        return float(bill_amount(rng))

    for payer_acct in rent_active:
        leases[payer_acct] = recurring_model.init_lease(
            recurring_policy,
            plan.seed,
            rng,
            payer_acct=payer_acct,
            start_date=plan.start_date,
            landlords=plan.counterparties.biller_accounts,
            base_rent_sampler=base_rent_draw,
        )

    for payday in plan.paydays:
        for payer_acct in rent_active:
            state = leases[payer_acct]
            while payday >= state.end:
                state = recurring_model.advance_lease(
                    recurring_policy,
                    plan.seed,
                    rng,
                    payer_acct=payer_acct,
                    now=state.end,
                    landlords=plan.counterparties.biller_accounts,
                    prev=state,
                    reset_rent_sampler=base_rent_draw,
                )
            leases[payer_acct] = state

            txn_ts = payday + timedelta(
                days=rng.int(0, 5),
                hours=rng.int(7, 22),
                minutes=rng.int(0, 60),
            )
            amount = recurring_model.rent_at(
                recurring_policy,
                plan.seed,
                payer_acct=payer_acct,
                state=state,
                pay_date=payday,
            )

            txns.append(
                txf.make(
                    TxnSpec(
                        src=payer_acct,
                        dst=state.landlord_acct,
                        amt=amount,
                        ts=txn_ts,
                        channel="rent",
                        is_fraud=0,
                        ring_id=-1,
                    )
                )
            )

    return txns
