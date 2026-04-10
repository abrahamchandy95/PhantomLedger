"""
Canonical persona identifiers.

Every persona assignment and lookup uses these constants.
Keeps the string literal in one place.
"""

STUDENT = "student"
RETIRED = "retired"
FREELANCER = "freelancer"
SMALLBIZ = "smallbiz"
HNW = "hnw"
SALARIED = "salaried"

ALL: tuple[str, ...] = (
    STUDENT,
    RETIRED,
    FREELANCER,
    SMALLBIZ,
    HNW,
    SALARIED,
)

DEFAULT = SALARIED

EARNERS: frozenset[str] = frozenset(
    {
        SALARIED,
        FREELANCER,
        SMALLBIZ,
        HNW,
    }
)
