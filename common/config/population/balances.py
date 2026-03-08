from dataclasses import dataclass

from common.validate import (
    require_float_between,
    require_float_ge,
)


@dataclass(frozen=True, slots=True)
class BalancesConfig:
    enable_balance_constraints: bool = True

    overdraft_frac: float = 0.05
    overdraft_limit_median: float = 300.0
    overdraft_limit_sigma: float = 0.6

    init_bal_student: float = 200.0
    init_bal_salaried: float = 1200.0
    init_bal_retired: float = 1500.0
    init_bal_freelancer: float = 900.0
    init_bal_smallbiz: float = 8000.0
    init_bal_hnw: float = 25000.0
    init_bal_sigma: float = 1.0

    def validate(self) -> None:
        require_float_between("overdraft_frac", self.overdraft_frac, 0.0, 1.0)
        require_float_ge("overdraft_limit_median", self.overdraft_limit_median, 0.0)
        require_float_ge("overdraft_limit_sigma", self.overdraft_limit_sigma, 0.0)
        require_float_ge("init_bal_sigma", self.init_bal_sigma, 0.0)
