from collections.abc import Iterable
from typing import Any, TypeVar

T = TypeVar("T")

_ENABLED = False

try:
    from tqdm.auto import tqdm
except Exception:  # pragma: no cover - tqdm is optional at import time
    tqdm = None


def enable(enabled: bool) -> None:
    global _ENABLED
    _ENABLED = enabled


def is_enabled() -> bool:
    return _ENABLED and tqdm is not None


def maybe_tqdm(iterable: Iterable[T], /, **kwargs: Any) -> Iterable[T]:
    if is_enabled():
        assert tqdm is not None
        return tqdm(iterable, **kwargs)
    return iterable


def status(message: str) -> None:
    if not is_enabled():
        return
    assert tqdm is not None
    tqdm.write(message)
