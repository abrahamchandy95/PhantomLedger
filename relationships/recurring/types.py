from collections.abc import Callable

from common.random import SeedBank

type SalarySampler = Callable[[], float]
type RentSampler = Callable[[], float]
type SeedSource = SeedBank | int
