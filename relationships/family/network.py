from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class FamilyGraph:
    households: list[list[str]]
    household_map: dict[str, int]

    spouses: dict[str, str]

    parents: dict[str, tuple[str, ...]]
    children: dict[str, list[str]]

    supported_parents: dict[str, tuple[str, ...]]
    supporting_children: dict[str, list[str]]
