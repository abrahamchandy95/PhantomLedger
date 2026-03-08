from dataclasses import dataclass

from common.validate import require_float_between


@dataclass(frozen=True, slots=True)
class PersonasConfig:
    persona_student_frac: float = 0.12
    persona_retired_frac: float = 0.10
    persona_freelancer_frac: float = 0.10
    persona_smallbiz_frac: float = 0.06
    persona_hnw_frac: float = 0.02

    def validate(self) -> None:
        items = {
            "persona_student_frac": self.persona_student_frac,
            "persona_retired_frac": self.persona_retired_frac,
            "persona_freelancer_frac": self.persona_freelancer_frac,
            "persona_smallbiz_frac": self.persona_smallbiz_frac,
            "persona_hnw_frac": self.persona_hnw_frac,
        }

        for name, value in items.items():
            require_float_between(name, value, 0.0, 1.0)

        if float(sum(map(float, items.values()))) > 1.0:
            raise ValueError("sum of persona_*_frac must be <= 1.0")
