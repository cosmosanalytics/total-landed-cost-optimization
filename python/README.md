# Total Landed Cost Optimization (Python) — Sourcing/Routing Assignment MIP

A Python 3 port of `total_landed_cost`, a mixed-integer programming (MIP)
model built around a shipping-cost regression, for data-driven sourcing and
routing decisions across a distribution network. It ports the algorithm
itself — the regression, the greedy heuristic, the exact solver, and the
full test suite — faithfully from the original C++ implementation.

## The problem

A company sources product from a set of candidate source plants (each with
a location, a unit production/procurement cost, and a capacity on how much
total weight it can supply) to fill a set of orders (each with a
destination location and a shipment weight). Every order must be assigned
to exactly one source.

The cost of serving an order from a source — its **landed cost** — is the
unit production cost times the order's weight, plus the cost of shipping
it from source to destination. Shipping cost is not a flat per-mile rate:
it is estimated from a **linear regression fit against historical shipment
records**, as a function of distance and weight — capturing base handling
fees, distance-driven fuel/transit cost, and weight-driven freight cost
all at once.

Sources carry **no fixed cost to activate** — every source plant is
already running; the only constraint is how much weight it can supply.
That makes this a **capacitated transportation/assignment problem**: the
goal is purely to find the weight-feasible order → source assignment
minimizing total landed cost.

### Shipping-cost regression

Given historical shipment records `(distance, weight, actualCost)`, fit

```
cost = b0 + b1 * distance + b2 * weight
```

by ordinary least squares. `b0` is a base handling fee, `b1` a per-mile
rate, `b2` a per-pound rate. The fitted model then prices every candidate
order/source pairing.

A three-parameter model (intercept + two linear rates) was chosen over a
four-parameter one with a `distance*weight` interaction term: the extra
term pushes the normal-equations matrix noticeably closer to singular on
small, unreplicated fixtures like the ones this project tests against — a
numerical-robustness cost not worth paying for a demo-scale model.

### MIP formulation

For orders `i = 1..n` and candidate sources `j = 1..m`:

- `x_ij ∈ {0,1}` — order `i` is served by source `j`
- `landedCost_ij = unitCost_j * weight_i + shippingModel.estimate(distance(i,j), weight_i)`

```
minimize   Σ_ij landedCost_ij * x_ij

subject to Σ_j x_ij = 1                          for every order i
           Σ_i weight_i * x_ij  ≤  capacity_j     for every source j
           x_ij ∈ {0, 1}
```

There is no "source opened" variable and no fixed-cost term — every source
is always available, so the capacity constraint is a plain weight-sum
bound rather than a big-M coupling constraint.

## Design

- **`landedcost.order.Order`**, **`landedcost.source_plant.SourcePlant`** —
  frozen dataclasses for orders and candidate source plants.
- **`landedcost.shipping_cost_model.ShippingCostModel`** — the regression:
  `estimate(distance, weight)`, and `fit()` performing ordinary least
  squares via the normal equations `(X^T X) b = X^T y`, solved with a
  hand-written Gaussian elimination (partial pivoting) — the exact same
  numerical routine as the C++ original, not `numpy.linalg.lstsq`.
- **`landedcost.problem.LandedCostProblem`** owns the orders, source
  plants, and shipping model; computes distances/landed costs and
  `validate()`s any candidate `LandedCostSolution` by independently
  recomputing feasibility (every order assigned exactly once, per-source
  capacity respected) and total cost from scratch.
- **`landedcost.solver.LandedCostSolver`** is a small `ABC` interface
  (`solve`, `name`) so `main.py` and the tests can swap backends freely.
- **`landedcost.greedy_solver.GreedyLowestCostSolver`** — a fast
  heaviest-order-first heuristic: place the hardest-to-fit orders first,
  always to the lowest-landed-cost source with remaining capacity.
- **`landedcost.exact_solver.BranchAndBoundSolver`** — a from-scratch
  exact solver with zero external dependencies. It branches over which
  source serves each order, seeds its incumbent from the greedy solution,
  and prunes with an admissible lower bound (cheapest possible landed
  cost, ignoring capacity, for every not-yet-assigned order).
- **`landedcost.pulp_solver.PulpMipSolver`** — the production-scale path,
  expressing the same MIP formulation directly against
  [PuLP](https://coin-or.github.io/pulp/) and solving with the bundled
  COIN-OR CBC binary. This mirrors the C++ project's `CbcMipSolver.h`,
  which is "documentary" (compiled only behind a `USE_CBC` build flag
  requiring system CBC libraries) — here the equivalent gate is simply
  `pip install pulp`. `pulp` is imported lazily inside
  `PulpMipSolver.solve()`, so importing the `landedcost` package never
  requires it to be installed.

The greedy and branch-and-bound solvers are dependency-free (Python
standard library only) and are what the test suite exercises by default.
The PuLP/CBC solver is the production-scale path for larger instances; its
test is automatically skipped if `pulp` isn't installed.

## Build & run

```sh
pip install -r requirements.txt   # optional -- only needed for the pulp solver/tests

python3 -m unittest discover -s tests -v
python3 main.py
```

## Layout

```
landedcost/    Order, SourcePlant, ShippingCostModel, LandedCostProblem,
               LandedCostSolver interface, GreedyLowestCostSolver,
               BranchAndBoundSolver, PulpMipSolver
tests/         test_landed_cost.py -- regression-fit recovery, feasibility
               checks, greedy-vs-exact comparison, edge cases, and a
               pulp-vs-exact cross-check (skipped if pulp isn't installed)
main.py        Demo: sample instance, greedy + exact (+ pulp if available)
```
