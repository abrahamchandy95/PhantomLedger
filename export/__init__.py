from .registry import export, register

_registered = False


def register_all() -> None:
    """Import all exporter modules to trigger @register decorators."""
    global _registered
    if _registered:
        return

    from . import standard
    from . import mule_ml
    from . import aml

    _registered = True


__all__ = [
    "export",
    "register",
    "register_all",
]
