from typing import TypeVar

T = TypeVar("T")


def swap_delete(
    pool: list[T],
    index_by_item: dict[T, int],
    item: T,
) -> None:
    """Remove an item from a list-backed pool in O(1) using swap-delete."""
    idx = index_by_item.pop(item)
    last_idx = len(pool) - 1
    last_item = pool[last_idx]

    if idx != last_idx:
        pool[idx] = last_item
        index_by_item[last_item] = idx

    _ = pool.pop()
