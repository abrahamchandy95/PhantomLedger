from typing import cast

import numpy as np

import entities.models as models
from common.math import F64, Scalar, as_float, as_int
from common.persona_names import SALARIED
from entities.personas import PERSONAS
from math_models.dormancy import Phase

from ..dynamics import PersonDynamics, PopulationDynamics
from ..environment import Market


FALLBACK_PERSONA = PERSONAS[SALARIED]


def build_sensitivities(market: Market) -> F64:
    persons = market.population.persons
    persona_objects = market.population.persona_objects

    out = np.empty(len(persons), dtype=np.float64)
    for i, person_id in enumerate(persons):
        persona: models.Persona = persona_objects.get(person_id, FALLBACK_PERSONA)
        out[i] = float(persona.paycheck_sensitivity)

    return out


def sync_dynamics(
    batch: PopulationDynamics,
    people: list[PersonDynamics],
) -> None:
    """
    Preserve the scalar source of truth after a batched run.
    """
    for i, person in enumerate(people):
        person.momentum.value = as_float(cast(Scalar, batch.momentum.values[i]))

        person.dormancy.phase = Phase(
            as_int(cast(int | np.integer, batch.dormancy.phase[i]))
        )
        person.dormancy.remaining = as_int(
            cast(int | np.integer, batch.dormancy.remaining[i])
        )
        person.dormancy.wake_total = as_int(
            cast(int | np.integer, batch.dormancy.wake_total[i])
        )

        person.paycheck_boost.boost = as_float(cast(Scalar, batch.paycheck.boost[i]))
        person.paycheck_boost.daily_decay = as_float(
            cast(Scalar, batch.paycheck.decay[i])
        )
        person.paycheck_boost.days_left = as_int(
            cast(int | np.integer, batch.paycheck.days_left[i])
        )
