"""GreedyLowestCostSolver: fast heaviest-order-first heuristic."""

from .problem import Assignment, LandedCostProblem, LandedCostSolution
from .solver import LandedCostSolver


class GreedyLowestCostSolver(LandedCostSolver):
    """Process orders heaviest-first (place the hardest-to-fit demand while
    capacity is still plentiful), assigning each order to the source with
    the lowest landed cost that still has enough remaining capacity. Falls
    back to the lowest-landed-cost source overall (ignoring capacity) if
    nothing fits, so the caller always gets a complete solution to
    inspect -- LandedCostProblem.validate() is what actually flags
    infeasibility.
    """

    def solve(self, problem: LandedCostProblem) -> LandedCostSolution:
        orders = problem.orders
        sources = problem.sources

        # Heaviest-first: place the orders that are hardest to fit while
        # capacity is still plentiful.
        order_indices = sorted(range(len(orders)), key=lambda i: orders[i].weight, reverse=True)

        remaining_capacity = {s.id: s.capacity for s in sources}

        solution = LandedCostSolution()

        for idx in order_indices:
            o = orders[idx]

            # Prefer the lowest-landed-cost source with enough remaining
            # capacity.
            chosen = None
            best_cost = float("inf")
            for source in sources:
                if remaining_capacity[source.id] + 1e-9 < o.weight:
                    continue
                c = problem.landed_cost(o, source)
                if c < best_cost:
                    best_cost = c
                    chosen = source

            # Nothing has room: fall back to the lowest-landed-cost source
            # overall so the solution stays complete; validate() will mark
            # it infeasible.
            if chosen is None:
                for source in sources:
                    c = problem.landed_cost(o, source)
                    if c < best_cost:
                        best_cost = c
                        chosen = source

            if chosen is None:
                continue  # no sources at all -- nothing we can do

            solution.assignments.append(Assignment(o.id, chosen.id))
            remaining_capacity[chosen.id] -= o.weight

        problem.validate(solution)
        return solution

    def name(self) -> str:
        return "Greedy-LowestCost"
