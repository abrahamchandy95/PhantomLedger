from dataclasses import Field, is_dataclass
from typing import Protocol, TypeAlias, TypeGuard, TypedDict, cast


Num: TypeAlias = int | float


class Meta(TypedDict, total=False):
    gt: Num
    ge: Num
    between: tuple[Num, Num]


class _DC(Protocol):
    __dataclass_fields__: dict[str, Field[object]]


def _is_dc(obj: object) -> TypeGuard[_DC]:
    return is_dataclass(obj) and not isinstance(obj, type)


def gt(name: str, v: Num, lo: Num) -> None:
    if v <= lo:
        raise ValueError(f"{name} must be > {lo}, got {v}")


def ge(name: str, v: Num, lo: Num) -> None:
    if v < lo:
        raise ValueError(f"{name} must be >= {lo}, got {v}")


def between(name: str, v: Num, lo: Num, hi: Num) -> None:
    if not (lo <= v <= hi):
        raise ValueError(f"{name} must be in [{lo}, {hi}], got {v}")


def _meta(f: Field[object]) -> Meta:
    return cast(Meta, cast(object, f.metadata))


def validate_metadata(obj: object) -> None:
    if not _is_dc(obj):
        raise TypeError("validate_metadata expects a dataclass instance")

    for f in obj.__dataclass_fields__.values():
        meta = _meta(f)
        if not meta:
            continue

        raw = cast(object, getattr(obj, f.name))
        if not isinstance(raw, (int, float)):
            raise TypeError(
                f"{type(obj).__name__}.{f.name} has validation metadata but is not numeric"
            )

        val: Num = raw

        if "gt" in meta:
            gt(f.name, val, meta["gt"])

        if "ge" in meta:
            ge(f.name, val, meta["ge"])

        if "between" in meta:
            lo, hi = meta["between"]
            between(f.name, val, lo, hi)
