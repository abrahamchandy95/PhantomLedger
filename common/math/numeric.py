import numpy as np
import numpy.typing as npt

type NumScalar = float | int | np.floating | np.integer

type ArrF64 = npt.NDArray[np.float64]
type ArrF32 = npt.NDArray[np.float32]
type ArrI64 = npt.NDArray[np.int64]
type ArrI32 = npt.NDArray[np.int32]
type ArrI16 = npt.NDArray[np.int16]
type ArrBool = npt.NDArray[np.bool_]
