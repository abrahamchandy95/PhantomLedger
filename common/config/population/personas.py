from dataclasses import dataclass, field

from common.validate import between


@dataclass(frozen=True, slots=True)
class Personas:
    fractions: dict[str, float] = field(
        default_factory=lambda: {
            "student": 0.12,
            "retired": 0.10,
            "freelancer": 0.10,
            "smallbiz": 0.06,
            "hnw": 0.02,
        }
    )
    default: str = "salaried"

    def __post_init__(self) -> None:
        for name, value in self.fractions.items():
            between(name, value, 0.0, 1.0)

        total = sum(self.fractions.values())
        if total > 1.0:
            raise ValueError(
                f"Sum of persona fractions must be <= 1.0, got {total:.4f}"
            )
