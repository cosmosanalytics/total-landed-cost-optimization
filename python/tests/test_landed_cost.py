"""Port of tests/LandedCostTests.cpp -- every hand-verified assertion."""

import unittest

from landedcost import (
    Assignment,
    BranchAndBoundSolver,
    GreedyLowestCostSolver,
    LandedCostProblem,
    LandedCostSolution,
    Order,
    ShipmentRecord,
    ShippingCostModel,
    SourcePlant,
)

try:
    import pulp  # noqa: F401

    PULP_AVAILABLE = True
except ImportError:
    PULP_AVAILABLE = False


def make_source(id_: int, x: float, y: float, capacity: float, unit_cost: float) -> SourcePlant:
    return SourcePlant(id_, f"Source{id_}", x, y, capacity, unit_cost)


def _capacity_crunch_problem() -> LandedCostProblem:
    # Source 1 cheaper but capped at 10; orders 6,5,5 -- greedy seats the 6
    # first (leaving 4, too little for a 5); optimal pairs the 5s instead.
    model = ShippingCostModel(b0=50.0, b1=0.8, b2=2.5)
    orders = [Order(1, 5.0, 5.0, 6.0), Order(2, 5.0, 5.0, 5.0), Order(3, 5.0, 5.0, 5.0)]
    sources = [make_source(1, 5.0, 5.0, 10.0, 1.0), make_source(2, 5.0, 5.0, 100.0, 5.0)]
    return LandedCostProblem(orders, sources, model)


class LandedCostTests(unittest.TestCase):
    def test_shipping_cost_model_estimate_matches_formula(self):
        model = ShippingCostModel(b0=50.0, b1=0.8, b2=2.5)
        distance = 120.0
        weight = 40.0
        expected = 50.0 + 0.8 * distance + 2.5 * weight
        self.assertAlmostEqual(model.estimate(distance, weight), expected, delta=1e-9)

    def test_shipping_cost_model_fit_recovers_known_coefficients(self):
        true_b0 = 42.0
        true_b1 = 1.35
        true_b2 = 3.1

        def true_cost(d, w):
            return true_b0 + true_b1 * d + true_b2 * w

        # Noiseless -> OLS recovers coefficients exactly.
        distance_weight = [(10.0, 100.0), (20.0, 60.0), (15.0, 200.0), (50.0, 30.0),
                            (5.0, 150.0), (35.0, 90.0), (80.0, 10.0)]
        records = [ShipmentRecord(d, w, true_cost(d, w)) for d, w in distance_weight]
        fitted = ShippingCostModel.fit(records)
        self.assertAlmostEqual(fitted.b0, true_b0, delta=1e-6)
        self.assertAlmostEqual(fitted.b1, true_b1, delta=1e-6)
        self.assertAlmostEqual(fitted.b2, true_b2, delta=1e-6)

    def test_feasibility_respects_capacity(self):
        model = ShippingCostModel(0.0, 0.0, 0.0)
        orders = [Order(1, 0.0, 0.0, 4.0), Order(2, 1.0, 0.0, 5.0)]
        sources = [make_source(1, 0.0, 0.0, 10.0, 1.0)]
        problem = LandedCostProblem(orders, sources, model)
        solution = LandedCostSolution(assignments=[Assignment(1, 1), Assignment(2, 1)])
        problem.validate(solution)
        self.assertTrue(solution.feasible)

    def test_feasibility_detects_capacity_violation(self):
        model = ShippingCostModel(0.0, 0.0, 0.0)
        orders = [Order(1, 0.0, 0.0, 6.0), Order(2, 1.0, 0.0, 7.0)]
        sources = [make_source(1, 0.0, 0.0, 10.0, 1.0)]
        problem = LandedCostProblem(orders, sources, model)
        solution = LandedCostSolution(assignments=[Assignment(1, 1), Assignment(2, 1)])
        problem.validate(solution)
        self.assertFalse(solution.feasible)

    def test_feasibility_detects_missing_order(self):
        model = ShippingCostModel(0.0, 0.0, 0.0)
        orders = [Order(1, 0.0, 0.0, 1.0), Order(2, 1.0, 0.0, 1.0)]
        sources = [make_source(1, 0.0, 0.0, 10.0, 1.0)]
        problem = LandedCostProblem(orders, sources, model)
        solution = LandedCostSolution(assignments=[Assignment(1, 1)])
        problem.validate(solution)
        self.assertFalse(solution.feasible)

    def test_greedy_produces_feasible_solution_when_capacity_allows(self):
        model = ShippingCostModel(10.0, 0.5, 0.1)
        orders = [
            Order(1, 0.0, 0.0, 3.0), Order(2, 1.0, 0.0, 4.0),
            Order(3, 50.0, 0.0, 3.0), Order(4, 51.0, 0.0, 4.0),
        ]
        sources = [make_source(1, 0.0, 0.0, 10.0, 2.0), make_source(2, 50.0, 0.0, 10.0, 2.0)]
        problem = LandedCostProblem(orders, sources, model)
        solver = GreedyLowestCostSolver()
        solution = solver.solve(problem)
        self.assertTrue(solution.feasible)
        self.assertEqual(len(solution.assignments), len(orders))

    def test_branch_and_bound_never_worse_than_greedy(self):
        problem = _capacity_crunch_problem()
        greedy_solution = GreedyLowestCostSolver().solve(problem)
        exact_solution = BranchAndBoundSolver().solve(problem)
        self.assertTrue(greedy_solution.feasible)
        self.assertTrue(exact_solution.feasible)
        self.assertLessEqual(exact_solution.total_cost, greedy_solution.total_cost + 1e-9)
        self.assertLess(exact_solution.total_cost, greedy_solution.total_cost - 1e-6)  # strictly better

    def test_edge_case_zero_orders(self):
        model = ShippingCostModel(10.0, 0.5, 0.1)
        orders = []
        sources = [make_source(1, 0.0, 0.0, 10.0, 1.0)]
        problem = LandedCostProblem(orders, sources, model)
        greedy_solution = GreedyLowestCostSolver().solve(problem)
        exact_solution = BranchAndBoundSolver().solve(problem)
        self.assertTrue(greedy_solution.feasible)
        self.assertEqual(greedy_solution.assignments, [])
        self.assertEqual(greedy_solution.total_cost, 0.0)
        self.assertTrue(exact_solution.feasible)
        self.assertEqual(exact_solution.assignments, [])
        self.assertEqual(exact_solution.total_cost, 0.0)

    def test_edge_case_infeasible_when_total_weight_exceeds_all_capacity(self):
        model = ShippingCostModel(10.0, 0.5, 0.1)
        orders = [Order(1, 0.0, 0.0, 20.0)]
        sources = [make_source(1, 0.0, 0.0, 10.0, 1.0)]
        problem = LandedCostProblem(orders, sources, model)
        self.assertFalse(GreedyLowestCostSolver().solve(problem).feasible)
        self.assertFalse(BranchAndBoundSolver().solve(problem).feasible)

    @unittest.skipUnless(PULP_AVAILABLE, "pulp not installed")
    def test_pulp_solver_matches_exact_solver(self):
        from landedcost.pulp_solver import PulpMipSolver

        problem = _capacity_crunch_problem()
        exact_solution = BranchAndBoundSolver().solve(problem)
        pulp_solution = PulpMipSolver().solve(problem)
        self.assertTrue(pulp_solution.feasible)
        self.assertAlmostEqual(pulp_solution.total_cost, exact_solution.total_cost, delta=1e-6)


if __name__ == "__main__":
    unittest.main()
