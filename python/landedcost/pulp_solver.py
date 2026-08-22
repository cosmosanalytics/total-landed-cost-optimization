"""PulpMipSolver: production-scale path using PuLP + COIN-OR CBC.

Mirrors the C++ project's CbcMipSolver.h formulation: variables x_ij in
{0,1} (order i served by source j), no fixed-cost "source opened" variable
since sources carry no activation cost, so the capacity constraint is a
plain weight-sum bound.

    minimize   sum_ij landedCost_ij * x_ij
    subject to sum_j x_ij = 1                      for every order i
               sum_i weight_i * x_ij <= capacity_j  for every source j
               x_ij in {0, 1}

pulp is imported lazily so importing the landedcost package never requires
it to be installed.
"""

from .problem import Assignment, LandedCostProblem, LandedCostSolution
from .solver import LandedCostSolver


class PulpMipSolver(LandedCostSolver):
    def solve(self, problem: LandedCostProblem) -> LandedCostSolution:
        try:
            import pulp
        except ImportError as exc:
            raise RuntimeError(
                "PulpMipSolver requires the 'pulp' package (pip install pulp)"
            ) from exc

        orders = problem.orders
        sources = problem.sources

        model = pulp.LpProblem("total_landed_cost", pulp.LpMinimize)

        # x[i][j] = 1 if order i is served by source j.
        x = {
            (i, j): pulp.LpVariable(f"x_{i}_{j}", cat="Binary")
            for i in range(len(orders))
            for j in range(len(sources))
        }

        model += pulp.lpSum(
            problem.landed_cost(orders[i], sources[j]) * x[i, j]
            for i in range(len(orders))
            for j in range(len(sources))
        )

        # sum_j x_ij = 1 for every order i.
        for i in range(len(orders)):
            model += pulp.lpSum(x[i, j] for j in range(len(sources))) == 1

        # sum_i weight_i*x_ij <= capacity_j for every source j.
        for j in range(len(sources)):
            model += (
                pulp.lpSum(orders[i].weight * x[i, j] for i in range(len(orders)))
                <= sources[j].capacity
            )

        model.solve(pulp.PULP_CBC_CMD(msg=False))

        result = LandedCostSolution()
        for i in range(len(orders)):
            for j in range(len(sources)):
                if pulp.value(x[i, j]) is not None and pulp.value(x[i, j]) > 0.5:
                    result.assignments.append(Assignment(orders[i].id, sources[j].id))

        problem.validate(result)
        return result

    def name(self) -> str:
        return "Pulp-CBC-MIP"
