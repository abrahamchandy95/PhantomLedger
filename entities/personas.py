from dataclasses import dataclass

from common.config import PersonasConfig
from common.rng import Rng


@dataclass(frozen=True, slots=True)
class PersonaSpec:
    name: str
    # Multiplies the base activity rate for day-to-day events
    rate_mult: float
    # Multiplies typical P2P amounts
    amount_mult: float
    # Which time-of-day profile to use (see math_models/timing.py)
    timing_profile: str


PERSONA_ORDER: tuple[str, ...] = (
    "student",
    "retired",
    "freelancer",
    "smallbiz",
    "hnw",
    "salaried",
)

PERSONAS: dict[str, PersonaSpec] = {
    "student": PersonaSpec(
        "student", rate_mult=0.7, amount_mult=0.7, timing_profile="consumer"
    ),
    "retired": PersonaSpec(
        "retired", rate_mult=0.6, amount_mult=0.9, timing_profile="consumer_day"
    ),
    "freelancer": PersonaSpec(
        "freelancer", rate_mult=1.1, amount_mult=1.1, timing_profile="consumer"
    ),
    "smallbiz": PersonaSpec(
        "smallbiz", rate_mult=2.4, amount_mult=1.8, timing_profile="business"
    ),
    "hnw": PersonaSpec(
        "hnw", rate_mult=1.3, amount_mult=2.8, timing_profile="consumer"
    ),
    "salaried": PersonaSpec(
        "salaried", rate_mult=1.0, amount_mult=1.0, timing_profile="consumer"
    ),
}


def assign_personas(
    pcfg: PersonasConfig, rng: Rng, persons: list[str]
) -> dict[str, str]:
    """
    Assign one persona to each person.
    Uses fixed fractions from config; remainder becomes 'salaried'.
    """
    n = len(persons)
    if n == 0:
        return {}

    fracs: dict[str, float] = {
        "student": float(pcfg.persona_student_frac),
        "retired": float(pcfg.persona_retired_frac),
        "freelancer": float(pcfg.persona_freelancer_frac),
        "smallbiz": float(pcfg.persona_smallbiz_frac),
        "hnw": float(pcfg.persona_hnw_frac),
    }

    # sanitize
    total = float(sum(fracs.values()))
    if total > 0.95:
        # prevent negative salaried
        scale = 0.95 / total
        for k in fracs:
            fracs[k] *= scale

    counts: dict[str, int] = {k: int(fracs[k] * n) for k in fracs}
    assigned_total = sum(counts.values())
    salaried_n = max(0, n - assigned_total)

    # sample without replacement in blocks
    remaining = list(persons)
    out: dict[str, str] = {}

    for pname in ("student", "retired", "freelancer", "smallbiz", "hnw"):
        k = counts[pname]
        if k <= 0:
            continue
        chosen = rng.choice_k(remaining, k, replace=False)
        chosen_set = set(chosen)
        for p in chosen:
            out[p] = pname
        remaining = [p for p in remaining if p not in chosen_set]

    # leftover -> salaried
    for p in remaining[:salaried_n]:
        out[p] = "salaried"

    return out
