# Total Landed Cost Optimization (C++) — Sourcing/Routing Assignment MIP

A C++17 reimplementation of "Total Landed Cost Optimization," a
mixed-integer programming (MIP) model built around a shipping-cost
regression, for data-driven sourcing and routing decisions across a
global distribution network. The original is a Python model that fits a
shipping-cost regression against historical shipment data and solves the
resulting assignment problem with COIN-OR/CBC via PuLP; this repo ports
the algorithm itself — the regression, the exact solver, and the tests —
independent of any particular solver binding.

It's a companion to
[`network-optimization-2`](https://github.com/cosmosanalytics/network-optimization-2)
(truck-load packing) and
[`network-optimization-3`](https://github.com/cosmosanalytics/network-optimization-3)
(customer clustering / territory assignment), and follows the same
structure: a solver-agnostic problem definition, a `Strategy` interface, a
fast heuristic, a dependency-free exact solver, and (behind a build flag)
a production path through COIN-OR CBC.

## The problem

A company sources product from a set of candidate source plants (each
with a location, a unit production/procurement cost, and a capacity on
how much total weight it can supply) to fill a set of orders (each with a
destination location and a shipment weight). Every order must be assigned
to exactly one source.

The cost of serving an order from a source — its **landed cost** — is the
unit production cost times the order's weight, plus the cost of shipping
it from source to destination. Shipping cost is not a flat per-mile rate:
it is estimated from a **linear regression fit against historical
shipment records**, as a function of distance and weight — capturing base
handling fees, distance-driven fuel/transit cost, and weight-driven
freight cost all at once.

Unlike `network-optimization-3`'s facility-location model, sources here
carry **no fixed cost to activate** — every source plant is already
running; the only constraint is how much weight it can supply. That makes
this a **capacitated transportation/assignment problem**, not a
facility-location problem: the goal is purely to find the
weight-feasible order → source assignment minimizing total landed cost.

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
small, unreplicated fixtures like the ones this project tests against —
a numerical-robustness cost not worth paying for a demo-scale model.

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

There is no `y_j` "source opened" variable and no fixed-cost term — every
source is always available, so the capacity constraint is a plain
weight-sum bound rather than the big-M coupling constraint a
facility-location model needs.

## Design

- **`ShippingCostModel`** — the regression: takes coefficients directly,
  `estimate(distance, weight)`, and a `fit()` performing ordinary least
  squares via the normal equations `(X^T X) b = X^T y`, solved with
  hand-written Gaussian elimination (partial pivoting).
- **`LandedCostProblem`** owns the orders, source plants, and shipping
  model; computes distances/landed costs and validates any candidate
  `LandedCostSolution` (feasibility + total cost).
- **`LandedCostSolver`** is a small `Strategy` interface (`solve`,
  `name`) so `main.cpp` and the tests can swap backends freely.
- **`GreedyLowestCostSolver`** — a fast heaviest-order-first heuristic:
  place the hardest-to-fit orders first, always to the lowest-landed-cost
  source with remaining capacity.
- **`BranchAndBoundSolver`** — a from-scratch exact solver with zero
  external dependencies. It branches over which source serves each order,
  seeds its incumbent from the greedy solution, and prunes with an
  admissible lower bound (cheapest possible landed cost, ignoring
  capacity, for every not-yet-assigned order).
- **`CbcMipSolver`** (behind `LANDEDCOST_USE_CBC`) documents the same
  model expressed directly against COIN-OR CBC's C++ API (`CbcModel`,
  `OsiClpSolverInterface`) — the production-scale path.

## Build & run

```sh
cmake -B build
cmake --build build

./build/landedcost_demo    # runs greedy + exact solvers on a sample instance
./build/landedcost_tests   # unit tests
```

To build with the real CBC backend instead of just documenting it:

```sh
sudo apt-get install coinor-libcbc-dev coinor-libclp-dev \
                      coinor-libosi-dev coinor-libcoinutils-dev
cmake -DUSE_CBC=ON -B build
cmake --build build
```

## Layout

```
include/   Order, SourcePlant, ShippingCostModel, LandedCostProblem,
           LandedCostSolver interface, GreedyLowestCostSolver,
           BranchAndBoundSolver, CbcMipSolver
src/       Implementations of the above + main.cpp
tests/     Dependency-free unit test harness (TestFramework.h) and
           LandedCostTests.cpp — regression-fit recovery, feasibility
           checks, greedy-vs-exact comparison, and edge cases
```
