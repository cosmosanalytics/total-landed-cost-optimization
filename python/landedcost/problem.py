"""LandedCostProblem: a capacitated transportation/assignment problem.

Every order must be assigned to exactly one candidate source plant,
minimizing total landed cost -- unit production/procurement cost plus a
regression-estimated shipping cost -- subject to each source's total
supplied-weight capacity. Sources carry no fixed opening cost: they are
always available, only capacity-limited.
"""

import math
from dataclasses import dataclass, field
from typing import List, Optional

from .order import Order
from .shipping_cost_model import ShippingCostModel
from .source_plant import SourcePlant


@dataclass
class Assignment:
    """One order routed to one source plant."""

    order_id: int
    source_id: int


@dataclass
class LandedCostSolution:
    """A candidate sourcing/routing plan produced by a solver. Solvers
    build one of these, then hand it to LandedCostProblem.validate() to
    check feasibility and price it.
    """

    feasible: bool = True
    total_cost: float = 0.0
    assignments: List[Assignment] = field(default_factory=list)


class LandedCostProblem:
    def __init__(
        self,
        orders: List[Order],
        sources: List[SourcePlant],
        shipping_model: ShippingCostModel,
    ) -> None:
        self.orders = orders
        self.sources = sources
        self.shipping_model = shipping_model

    def distance(self, order: Order, source: SourcePlant) -> float:
        """Straight-line distance between an order's destination and a source."""
        dx = order.x - source.x
        dy = order.y - source.y
        return math.sqrt(dx * dx + dy * dy)

    def landed_cost(self, order: Order, source: SourcePlant) -> float:
        """Total landed cost of serving one order from one source: unit
        cost times weight, plus the regression-estimated shipping cost for
        that distance and weight.
        """
        production_cost = source.unit_cost * order.weight
        shipping_cost = self.shipping_model.estimate(self.distance(order, source), order.weight)
        return production_cost + shipping_cost

    def find_source(self, source_id: int) -> Optional[SourcePlant]:
        for s in self.sources:
            if s.id == source_id:
                return s
        return None

    def find_order(self, order_id: int) -> Optional[Order]:
        for o in self.orders:
            if o.id == order_id:
                return o
        return None

    def validate(self, solution: LandedCostSolution) -> None:
        """Recomputes feasibility and total_cost for a solution built by a
        solver: every order must appear exactly once, referencing a known
        source; per-source total assigned weight must not exceed capacity
        (+1e-9 tolerance); cost is the sum of landed_cost over all
        assignments if feasible, else 0.
        """
        times_seen = {o.id: 0 for o in self.orders}
        used_capacity = {s.id: 0.0 for s in self.sources}

        feasible = True
        cost = 0.0

        for a in solution.assignments:
            order = self.find_order(a.order_id)
            source = self.find_source(a.source_id)
            if order is None or source is None:
                feasible = False
                continue
            times_seen[order.id] = times_seen.get(order.id, 0) + 1
            used_capacity[source.id] = used_capacity.get(source.id, 0.0) + order.weight
            cost += self.landed_cost(order, source)

        for count in times_seen.values():
            if count != 1:
                feasible = False  # every order exactly once

        for s in self.sources:
            if used_capacity[s.id] > s.capacity + 1e-9:
                feasible = False

        solution.feasible = feasible
        solution.total_cost = cost if feasible else 0.0
