from datetime import datetime, timedelta
from math import pow
from typing import cast

import numpy as np

from common.config import FamilyConfig, WindowConfig
from common.probability import as_int
from common.rng import Rng
from common.seeding import derived_seed
from common.temporal import iter_month_starts
from common.types import Txn
from entities.merchants import MerchantData
from relationships.family import FamilyData
from transfers.txns import TxnFactory, TxnSpec


def pareto_amount(rng: Rng, *, xm: float, alpha: float) -> float:
    """
    Pareto Type I sample:
        X = xm * (1 - U)^(-1 / alpha)
    where U ~ Uniform(0, 1).
    """
    u = float(rng.float())
    scale = pow(1.0 - u, -1.0 / float(alpha))
    amount = float(xm) * float(scale)
    return round(max(float(xm), amount), 2)


def _pick_education_payee(
    merchants: MerchantData,
    gen: np.random.Generator,
) -> str | None:
    education_accounts = [
        acct
        for acct, cat in zip(merchants.counterparty_acct, merchants.category)
        if cat == "education"
    ]
    if not education_accounts:
        return None

    idx = as_int(cast(int | np.integer, gen.integers(0, len(education_accounts))))
    return education_accounts[idx]


def _support_capacity_weight(persona: str) -> float:
    match persona:
        case "hnw":
            return 2.2
        case "smallbiz":
            return 1.5
        case "freelancer":
            return 0.95
        case "salaried":
            return 1.0
        case _:
            return 0.75


def _weighted_pick_person(
    people: list[str],
    persona_for_person: dict[str, str],
    gen: np.random.Generator,
) -> str:
    if len(people) == 1:
        return people[0]

    weights = [
        _support_capacity_weight(persona_for_person.get(person_id, "salaried"))
        for person_id in people
    ]
    total = float(sum(weights))

    if total <= 0.0:
        idx = as_int(cast(int | np.integer, gen.integers(0, len(people))))
        return people[idx]

    u = float(gen.random()) * total
    acc = 0.0
    for person_id, weight in zip(people, weights):
        acc += float(weight)
        if u <= acc:
            return person_id

    return people[-1]


def _window_month_starts(start_date: datetime, days: int) -> list[datetime]:
    """
    Month anchors restricted to the simulation window.
    This fixes the bug where iter_month_starts() can yield the first day
    of the starting month even when that date is before start_date.
    """
    return [dt for dt in iter_month_starts(start_date, days) if dt >= start_date]


def generate_family_transfers(
    window: WindowConfig,
    fcfg: FamilyConfig,
    rng: Rng,
    *,
    base_seed: int,
    family: FamilyData,
    persona_for_person: dict[str, str],
    primary_acct_for_person: dict[str, str],
    merchants: MerchantData,
    infra_factory: TxnFactory,
) -> list[Txn]:
    start_date = window.start_date()
    days = int(window.days)
    end_excl = start_date + timedelta(days=days)

    txns: list[Txn] = []

    # Local deterministic generator for stable schedules/choices.
    gen = np.random.default_rng(derived_seed(base_seed, "family", "transfers"))

    month_starts = _window_month_starts(start_date, days)
    if not month_starts:
        month_starts = [start_date]

    # -------------------------
    # Allowances: parent -> student
    # -------------------------
    if fcfg.allowance_enabled:
        for child, parents in family.parents_of.items():
            if persona_for_person.get(child) != "student":
                continue

            child_acct = primary_acct_for_person.get(child)
            if child_acct is None:
                continue

            weekly = float(gen.random()) < float(fcfg.allowance_weekly_p)
            step_days = 7 if weekly else 14

            t = start_date + timedelta(
                days=as_int(cast(int | np.integer, gen.integers(0, 14))),
                hours=as_int(cast(int | np.integer, gen.integers(7, 21))),
                minutes=as_int(cast(int | np.integer, gen.integers(0, 60))),
            )

            while t < end_excl:
                payer_idx = as_int(
                    cast(int | np.integer, gen.integers(0, len(parents)))
                )
                payer_person = parents[payer_idx]
                payer_acct = primary_acct_for_person.get(payer_person)

                if payer_acct is not None and payer_acct != child_acct:
                    amt = pareto_amount(
                        rng,
                        xm=float(fcfg.allowance_pareto_xm),
                        alpha=float(fcfg.allowance_pareto_alpha),
                    )
                    txns.append(
                        infra_factory.make(
                            TxnSpec(
                                src=payer_acct,
                                dst=child_acct,
                                amt=amt,
                                ts=t,
                                channel="allowance",
                            )
                        )
                    )

                jitter = as_int(cast(int | np.integer, gen.integers(-1, 2)))
                t = t + timedelta(days=step_days + jitter)

    # -------------------------
    # Tuition: parent -> education biller
    # -------------------------
    if fcfg.tuition_enabled:
        payee = _pick_education_payee(merchants, gen)
        if payee is not None:
            students = [
                person_id
                for person_id, persona in persona_for_person.items()
                if persona == "student"
            ]

            for student_id in students:
                if student_id not in family.parents_of:
                    continue
                if float(gen.random()) >= float(fcfg.tuition_students_p):
                    continue

                parents = family.parents_of[student_id]
                payer_idx = as_int(
                    cast(int | np.integer, gen.integers(0, len(parents)))
                )
                payer_person = parents[payer_idx]
                payer_acct = primary_acct_for_person.get(payer_person)
                if payer_acct is None:
                    continue

                n_inst = as_int(
                    cast(
                        int | np.integer,
                        gen.integers(
                            int(fcfg.tuition_installments_min),
                            int(fcfg.tuition_installments_max) + 1,
                        ),
                    )
                )

                total = float(
                    gen.lognormal(
                        mean=float(fcfg.tuition_total_logn_mu),
                        sigma=float(fcfg.tuition_total_logn_sigma),
                    )
                )
                total = round(max(200.0, total), 2)
                installment_amount = round(total / max(1, n_inst), 2)

                semester_start = month_starts[0] + timedelta(
                    days=as_int(cast(int | np.integer, gen.integers(0, 10)))
                )

                for installment_idx in range(n_inst):
                    ts = semester_start + timedelta(days=30 * installment_idx)
                    if ts >= end_excl:
                        break

                    ts = ts + timedelta(
                        days=as_int(cast(int | np.integer, gen.integers(0, 5))),
                        hours=as_int(cast(int | np.integer, gen.integers(8, 18))),
                        minutes=as_int(cast(int | np.integer, gen.integers(0, 60))),
                    )
                    if ts >= end_excl:
                        break

                    noise = float(gen.normal(loc=0.0, scale=0.03))
                    amt = round(max(10.0, installment_amount * (1.0 + noise)), 2)

                    txns.append(
                        infra_factory.make(
                            TxnSpec(
                                src=payer_acct,
                                dst=payee,
                                amt=amt,
                                ts=ts,
                                channel="tuition",
                            )
                        )
                    )

    # -------------------------
    # Support: adult child -> retired parent
    # -------------------------
    if fcfg.retiree_support_enabled:
        retirees = [
            person_id
            for person_id, persona in persona_for_person.items()
            if persona == "retired"
        ]

        for retiree_id in retirees:
            adult_kids = family.adult_children_of.get(retiree_id)
            if not adult_kids:
                continue

            retiree_acct = primary_acct_for_person.get(retiree_id)
            if retiree_acct is None:
                continue

            for month_start in month_starts:
                if float(gen.random()) >= float(fcfg.retiree_support_p):
                    continue

                payer_person = _weighted_pick_person(
                    adult_kids,
                    persona_for_person,
                    gen,
                )
                payer_acct = primary_acct_for_person.get(payer_person)
                if payer_acct is None or payer_acct == retiree_acct:
                    continue

                ts = month_start + timedelta(
                    days=as_int(cast(int | np.integer, gen.integers(0, 6))),
                    hours=as_int(cast(int | np.integer, gen.integers(7, 21))),
                    minutes=as_int(cast(int | np.integer, gen.integers(0, 60))),
                )
                if ts >= end_excl:
                    break

                base_amt = pareto_amount(
                    rng,
                    xm=float(fcfg.retiree_support_pareto_xm),
                    alpha=float(fcfg.retiree_support_pareto_alpha),
                )
                mult = _support_capacity_weight(
                    persona_for_person.get(payer_person, "salaried")
                )
                amt = round(max(5.0, base_amt * mult), 2)

                txns.append(
                    infra_factory.make(
                        TxnSpec(
                            src=payer_acct,
                            dst=retiree_acct,
                            amt=amt,
                            ts=ts,
                            channel="family_support",
                        )
                    )
                )

    return txns
