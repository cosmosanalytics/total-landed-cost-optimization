"""Order: a customer order to be sourced."""

from dataclasses import dataclass


@dataclass(frozen=True)
class Order:
    """A destination location and a shipment weight to be sourced from a
    plant. Coordinates are a flat-plane approximation (e.g. projected
    miles/km) -- good enough for a sourcing/routing design model.
    """

    id: int
    x: float
    y: float
    weight: float
