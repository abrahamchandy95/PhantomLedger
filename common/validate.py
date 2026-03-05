def require_int_gt(name: str, v: int, lo: int) -> None:
    if int(v) <= lo:
        raise ValueError(f"{name} must be > {lo}")


def require_int_ge(name: str, v: int, lo: int) -> None:
    if int(v) < lo:
        raise ValueError(f"{name} must be >= {lo}")


def require_float_gt(name: str, v: float, lo: float) -> None:
    if float(v) <= lo:
        raise ValueError(f"{name} must be > {lo}")


def require_float_ge(name: str, v: float, lo: float) -> None:
    if float(v) < lo:
        raise ValueError(f"{name} must be >= {lo}")


def require_float_between(name: str, v: float, lo: float, hi: float) -> None:
    x = float(v)
    if not (lo <= x <= hi):
        raise ValueError(f"{name} must be between {lo} and {hi}")
