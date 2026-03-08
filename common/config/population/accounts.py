from dataclasses import dataclass

from common.validate import require_int_gt


@dataclass(frozen=True, slots=True)
class AccountsConfig:
    max_accounts_per_person: int = 3

    def validate(self) -> None:
        require_int_gt("max_accounts_per_person", self.max_accounts_per_person, 0)
