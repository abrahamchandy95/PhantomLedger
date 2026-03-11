from dataclasses import dataclass

from common.validate import require_float_between


@dataclass(frozen=True, slots=True)
class PersonasConfig:
    student_frac: float = 0.12
    retired_frac: float = 0.10
    freelancer_frac: float = 0.10
    smallbiz_frac: float = 0.06
    hnw_frac: float = 0.02

    def validate(self) -> None:
        items = {
            "student_frac": self.student_frac,
            "retired_frac": self.retired_frac,
            "freelancer_frac": self.freelancer_frac,
            "persona_smallbiz_frac": self.smallbiz_frac,
            "hnw_frac": self.hnw_frac,
        }

        for name, value in items.items():
            require_float_between(name, value, 0.0, 1.0)

        if float(sum(map(float, items.values()))) > 1.0:
            raise ValueError("sum of persona_*_frac must be <= 1.0")
