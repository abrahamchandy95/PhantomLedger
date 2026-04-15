from dataclasses import dataclass, field

from transfers.balances import ClearingHouse


@dataclass(slots=True)
class ScreenBook:
    initial_book: ClearingHouse | None
    _scratch: ClearingHouse | None = field(init=False, repr=False)

    def __post_init__(self) -> None:
        self._scratch = None if self.initial_book is None else self.initial_book.copy()

    def fresh(self) -> ClearingHouse | None:
        if self.initial_book is None or self._scratch is None:
            return None

        self._scratch.restore_from(self.initial_book)
        return self._scratch
