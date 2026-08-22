"""SourcePlant: a candidate source plant (production or procurement site)."""

from dataclasses import dataclass


@dataclass(frozen=True)
class SourcePlant:
    """A candidate source plant. Sources have no fixed "opening" cost --
    unlike a facility-location hub, every source is always available, just
    capacity-limited on total weight it can supply across all orders
    assigned to it.
    """

    id: int
    name: str
    x: float
    y: float
    capacity: float
    unit_cost: float
