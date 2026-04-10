from typing import SupportsFloat

import numpy as np
import numpy.typing as npt

type Scalar = float | int | np.floating | np.integer

type F64 = npt.NDArray[np.float64]
type F32 = npt.NDArray[np.float32]
type I64 = npt.NDArray[np.int64]
type I32 = npt.NDArray[np.int32]
type I16 = npt.NDArray[np.int16]
type Bool = npt.NDArray[np.bool_]


def as_float(x: SupportsFloat) -> float:
    return float(x)


def as_int(x: int | np.integer) -> int:
    return int(x)


def to_f64(values: npt.ArrayLike) -> F64:
    return np.asarray(values, dtype=np.float64)


def to_i64(values: npt.ArrayLike) -> I64:
    return np.asarray(values, dtype=np.int64)
