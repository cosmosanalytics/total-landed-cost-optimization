"""Demo: build a sample sourcing/routing instance and compare solvers."""

from collections import defaultdict

from landedcost import (
    BranchAndBoundSolver,
    GreedyLowestCostSolver,
    LandedCostProblem,
    LandedCostSolution,
    Order,
    ShippingCostModel,
    SourcePlant,
)


def print_solution(solver_name: str, problem: LandedCostProblem, solution: LandedCostSolution) -> None:
    print(f"\n--- {solver_name} ---")
    print(f"Feasible: {'yes' if solution.feasible else 'no'}")
    print(f"Total landed cost: ${solution.total_cost:.2f}")

    orders_by_source = defaultdict(list)
    weight_by_source = defaultdict(float)
    for a in solution.assignments:
        orders_by_source[a.source_id].append(a.order_id)
        order = problem.find_order(a.order_id)
        if order:
            weight_by_source[a.source_id] += order.weight

    for source in problem.sources:
        if source.id not in orders_by_source:
            continue
        ids = orders_by_source[source.id]
        print(
            f"  {source.name} (load {weight_by_source[source.id]} / "
            f"{source.capacity} capacity): orders {{{', '.join(str(i) for i in ids)}}}"
        )


def main() -> None:
    # Shipping-cost regression: cost = b0 + b1*distance + b2*weight, i.e. a
    # base handling fee plus a per-mile rate and a per-pound rate. In
    # practice this model would be fit with ShippingCostModel.fit() against
    # historical shipment records; here we use plausible hand-picked
    # coefficients for the demo.
    shipping_model = ShippingCostModel(b0=50.0, b1=0.8, b2=2.5)

    # Orders spread across two regions (destinations in projected miles),
    # weight in pounds.
    orders = [
        Order(1, 10.0, 10.0, 120.0), Order(2, 15.0, 8.0, 90.0),
        Order(3, 12.0, 20.0, 200.0), Order(4, 8.0, 15.0, 60.0),
        Order(5, 300.0, 210.0, 150.0), Order(6, 310.0, 200.0, 80.0),
        Order(7, 295.0, 220.0, 250.0), Order(8, 305.0, 215.0, 100.0),
        Order(9, 150.0, 100.0, 175.0), Order(10, 20.0, 5.0, 130.0),
    ]

    # Candidate source plants with differing unit cost, capacity, and
    # location.
    sources = [
        SourcePlant(1, "West Coast Plant", 0.0, 0.0, 500.0, 4.00),
        SourcePlant(2, "Midwest Plant", 150.0, 90.0, 450.0, 3.50),
        SourcePlant(3, "East Coast Plant", 320.0, 210.0, 500.0, 4.25),
        SourcePlant(4, "Overseas Supplier", 0.0, 400.0, 400.0, 2.75),
    ]

    problem = LandedCostProblem(orders, sources, shipping_model)

    print("Total Landed Cost Optimization (Python) -- Sourcing/Routing Assignment MIP")
    print(f"{len(orders)} orders, {len(sources)} candidate source plants")
    print(
        f"Shipping model: cost = {shipping_model.b0} + {shipping_model.b1}"
        f"*distance + {shipping_model.b2}*weight"
    )

    greedy = GreedyLowestCostSolver()
    print_solution(greedy.name(), problem, greedy.solve(problem))

    exact = BranchAndBoundSolver()
    print_solution(exact.name() + " (exact)", problem, exact.solve(problem))

    try:
        from landedcost.pulp_solver import PulpMipSolver

        pulp_solver = PulpMipSolver()
        print_solution(pulp_solver.name(), problem, pulp_solver.solve(problem))
    except RuntimeError:
        print("\n(pulp not installed -- skipping Pulp-CBC-MIP solver)")


if __name__ == "__main__":
    main()
