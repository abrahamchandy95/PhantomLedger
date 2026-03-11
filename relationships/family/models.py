from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class FamilyData:
    households: list[list[str]]
    household_id_for_person: dict[str, int]

    spouse_of: dict[str, str]  # p -> spouse

    # Dependent-child relationships
    parents_of: dict[str, tuple[str, ...]]  # dependent child -> parents
    children_of: dict[str, list[str]]  # parent -> dependent children

    # Adult-child support relationships for retirees
    adult_parents_of: dict[str, tuple[str, ...]]  # adult child -> retired parent(s)
    adult_children_of: dict[str, list[str]]  # retired parent -> adult child(ren)
