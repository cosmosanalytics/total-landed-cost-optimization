"""BranchAndBoundSolver: exact, dependency-free capacitated assignment solver."""

from dataclasses import dataclass, field
from typing import List

from .greedy_solver import GreedyLowestCostSolver
from .problem import Assignment, LandedCostProblem, LandedCostSolution
from .solver import LandedCostSolver


@dataclass
class _SearchState:
    problem: LandedCostProblem
    orders: list
    sources: list
    cheapest_possible_cost: List[float] = field(default_factory=list)
    suffix_lower_bound: List[float] = field(default_factory=list)
    assignment: List[int] = field(default_factory=list)
    remaining: List[float] = field(default_factory=list)
    best_assignment: List[int] = field(default_factory=list)
    best_cost: float = float("inf")


def _search(s: _SearchState, order_idx: int, running_cost: float) -> None:
    n = len(s.orders)
    if order_idx == n:
        if running_cost < s.best_cost:
            s.best_cost = running_cost
            s.best_assignment = s.assignment[:]
        return

    if running_cost + s.suffix_lower_bound[order_idx] >= s.best_cost:
        return  # even the most optimistic completion can't beat the incumbent

    order = s.orders[order_idx]

    # Try cheaper sources first so a good incumbent is found early, which
    # makes the lower-bound prune above effective sooner.
    source_order = sorted(
        range(len(s.sources)),
        key=lambda j: s.problem.landed_cost(order, s.sources[j]),
    )

    for j in source_order:
        source = s.sources[j]
        if s.remaining[j] + 1e-9 < order.weight:
            continue

        added_cost = s.problem.landed_cost(order, source)

        s.assignment[order_idx] = j
        s.remaining[j] -= order.weight

        _search(s, order_idx + 1, running_cost + added_cost)

        s.remaining[j] += order.weight

    s.assignment[order_idx] = -1


class BranchAndBoundSolver(LandedCostSolver):
    """Branches over which source serves each order, tracking per-source
    remaining capacity. Prunes with an admissible lower bound: the
    cheapest possible landed cost across all sources, ignoring capacity,
    for every not-yet-assigned order.

    Exponential in the worst case, meant for the small/medium instances a
    portfolio demo or a regression test uses -- pulp_solver.py is the path
    for production scale.
    """

    def solve(self, problem: LandedCostProblem) -> LandedCostSolution:
        orders = problem.orders
        sources = problem.sources

        if not orders:
            return LandedCostSolution(feasible=True, total_cost=0.0)

        s = _SearchState(problem=problem, orders=orders, sources=sources)
        s.assignment = [-1] * len(orders)
        s.remaining = [src.capacity for src in sources]

        # Admissible lower bound: cheapest single-source landed cost per
        # order, ignoring capacity.
        s.cheapest_possible_cost = []
        for o in orders:
            if sources:
                best = min(problem.landed_cost(o, src) for src in sources)
            else:
                best = 0.0
            s.cheapest_possible_cost.append(best)

        s.suffix_lower_bound = [0.0] * (len(orders) + 1)
        for i in range(len(orders) - 1, -1, -1):
            s.suffix_lower_bound[i] = s.suffix_lower_bound[i + 1] + s.cheapest_possible_cost[i]

        # Seed the incumbent with the greedy heuristic's result so pruning
        # is effective from the very first branch instead of only after
        # the search stumbles onto a decent solution by luck.
        greedy = GreedyLowestCostSolver()
        greedy_solution = greedy.solve(problem)
        if greedy_solution.feasible:
            s.best_cost = greedy_solution.total_cost

        if sources:
            _search(s, 0, 0.0)

        if not s.best_assignment and not greedy_solution.feasible:
            # No feasible assignment exists at all (e.g. total weight
            # exceeds total capacity) -- report the infeasible greedy
            # attempt so the caller can see how far off it was.
            return greedy_solution
        if not s.best_assignment:
            return greedy_solution  # greedy already found the (only) feasible answer

        result = LandedCostSolution()
        for i, o in enumerate(orders):
            source_idx = s.best_assignment[i]
            result.assignments.append(Assignment(o.id, sources[source_idx].id))

        problem.validate(result)
        return result

    def name(self) -> str:
        return "BranchAndBound-Exact"
