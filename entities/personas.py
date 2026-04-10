from common import config
from common.math import lognormal_by_median
from common.persona_names import STUDENT, RETIRED, FREELANCER, SMALLBIZ, HNW, SALARIED
from common.random import Rng, RngFactory

from . import models


PERSONAS: dict[str, models.Persona] = {
    STUDENT: models.Persona(
        name=STUDENT,
        rate_multiplier=0.7,
        amount_multiplier=0.7,
        timing_profile="consumer",
        initial_balance=200.0,
        card_prob=0.25,
        cc_share=0.55,
        credit_limit=800.0,
        weight=0.18,
        paycheck_sensitivity=0.67,
    ),
    RETIRED: models.Persona(
        name=RETIRED,
        rate_multiplier=0.6,
        amount_multiplier=0.9,
        timing_profile="consumer_day",
        initial_balance=1500.0,
        card_prob=0.55,
        cc_share=0.55,
        credit_limit=2500.0,
        weight=0.3,
        paycheck_sensitivity=0.50,
    ),
    FREELANCER: models.Persona(
        name=FREELANCER,
        rate_multiplier=1.1,
        amount_multiplier=1.1,
        timing_profile="consumer",
        initial_balance=900.0,
        card_prob=0.65,
        cc_share=0.65,
        credit_limit=4000.0,
        weight=0.95,
        paycheck_sensitivity=0.33,
    ),
    SMALLBIZ: models.Persona(
        name=SMALLBIZ,
        rate_multiplier=2.4,
        amount_multiplier=1.8,
        timing_profile="business",
        initial_balance=8000.0,
        card_prob=0.80,
        cc_share=0.75,
        credit_limit=7000.0,
        weight=1.5,
        paycheck_sensitivity=0.29,
    ),
    HNW: models.Persona(
        name=HNW,
        rate_multiplier=1.3,
        amount_multiplier=2.8,
        timing_profile="consumer",
        initial_balance=25000.0,
        card_prob=0.92,
        cc_share=0.80,
        credit_limit=15000.0,
        weight=2.2,
        paycheck_sensitivity=0.11,
    ),
    SALARIED: models.Persona(
        name=SALARIED,
        rate_multiplier=1.0,
        amount_multiplier=1.0,
        timing_profile="consumer",
        initial_balance=1200.0,
        card_prob=0.70,
        cc_share=0.70,
        credit_limit=3000.0,
        weight=1.0,
        paycheck_sensitivity=0.40,
    ),
}

_DEFAULT_PERSONA = PERSONAS[SALARIED]

_PAYCHECK_SENSITIVITY_BETA: dict[str, tuple[float, float]] = {
    STUDENT: (4.0, 2.0),
    RETIRED: (3.0, 3.0),
    SALARIED: (2.0, 3.0),
    FREELANCER: (2.0, 4.0),
    SMALLBIZ: (2.0, 5.0),
    HNW: (1.0, 8.0),
}


def get_persona(name: str) -> models.Persona:
    """Look up a persona by name, falling back to salaried."""
    return PERSONAS.get(name, _DEFAULT_PERSONA)


def individualize(
    archetype: models.Persona,
    rng_factory: RngFactory,
    person_id: str,
) -> models.Persona:
    """
    Sample per-person behavioral parameters from distributions
    centered on the archetype.

    Each numeric field gets a lognormal perturbation with small sigma,
    so the archetype is the median and individuals scatter around it.
    This means two salaried people might have rate_multipliers of
    0.87 and 1.14 instead of both being exactly 1.0.

    Fields that are probabilities get clamped to [0, 1].
    Fields that are categorical (name, timing_profile) stay fixed —
    a salaried person doesn't randomly become a business-hours spender.
    """
    gen = rng_factory.rng("persona_individual", person_id).gen

    # Small sigma: individuals vary ~15-25% around the archetype.
    # This matches observed within-group variation in spending
    # behavior studies (Vilella et al. 2021, EPJ Data Science 10:25).
    sigma = 0.15

    def _perturb(median: float) -> float:
        if median <= 0.0:
            return median
        return float(lognormal_by_median(gen, median=median, sigma=sigma))

    def _perturb_prob(p: float) -> float:
        if p <= 0.0:
            return 0.0
        if p >= 1.0:
            return 1.0
        # Use beta-like perturbation: shift by a small normal amount
        noise = float(gen.normal(loc=0.0, scale=0.08))
        return max(0.01, min(0.99, p + noise))

    def _sample_paycheck_sensitivity() -> float:
        alpha, beta = _PAYCHECK_SENSITIVITY_BETA.get(
            archetype.name,
            (2.0, 3.0),
        )
        return float(gen.beta(alpha, beta))

    return models.Persona(
        name=archetype.name,
        rate_multiplier=max(0.1, _perturb(archetype.rate_multiplier)),
        amount_multiplier=max(0.1, _perturb(archetype.amount_multiplier)),
        timing_profile=archetype.timing_profile,
        initial_balance=max(10.0, _perturb(archetype.initial_balance)),
        card_prob=_perturb_prob(archetype.card_prob),
        cc_share=_perturb_prob(archetype.cc_share),
        credit_limit=max(200.0, _perturb(archetype.credit_limit)),
        weight=max(0.01, _perturb(archetype.weight)),
        paycheck_sensitivity=_sample_paycheck_sensitivity(),
    )


def assign(pcfg: config.Personas, rng: Rng, persons: list[str]) -> dict[str, str]:
    """
    Assign one persona to each person.
    Uses fracs from config; remainder gets default_persona.
    """
    n = len(persons)
    if n == 0:
        return {}

    fracs = dict(pcfg.fractions)

    total = sum(fracs.values())
    if total > 0.95:
        scale = 0.95 / total
        fracs = {k: v * scale for k, v in fracs.items()}

    counts = {k: int(v * n) for k, v in fracs.items()}

    remaining = list(persons)
    out: dict[str, str] = {}

    for pname, k in counts.items():
        if k <= 0:
            continue
        chosen = rng.choice_k(remaining, k, replace=False)
        chosen_set = set(chosen)
        for p in chosen:
            out[p] = pname
        remaining = [p for p in remaining if p not in chosen_set]

    for p in remaining:
        out[p] = pcfg.default

    return out


def build_persona_objects(
    persona_map: dict[str, str],
    base_seed: int,
) -> dict[str, models.Persona]:
    """
    Build individualized Persona objects for every person.

    Each person gets their own Persona with parameters sampled
    around the archetype's values. The RNG is seeded deterministically
    per person, so results are reproducible regardless of iteration order.
    """
    rng_factory = RngFactory(base_seed)
    result: dict[str, models.Persona] = {}

    for person_id, persona_name in persona_map.items():
        archetype = get_persona(persona_name)
        result[person_id] = individualize(archetype, rng_factory, person_id)

    return result
