#include <cmath>
#include <vector>

#include "BranchAndBoundSolver.h"
#include "GreedyLowestCostSolver.h"
#include "LandedCostProblem.h"
#include "ShippingCostModel.h"
#include "TestFramework.h"

using namespace landedcost;

namespace {
SourcePlant makeSource(int id, double x, double y, double capacity, double unitCost) {
    return SourcePlant(id, "Source" + std::to_string(id), x, y, capacity, unitCost);
}
} // namespace

TEST(ShippingCostModel_EstimateMatchesFormula) {
    ShippingCostModel model(/*b0=*/50.0, /*b1=*/0.8, /*b2=*/2.5);
    const double distance = 120.0;
    const double weight = 40.0;
    const double expected = 50.0 + 0.8 * distance + 2.5 * weight;
    CHECK(std::abs(model.estimate(distance, weight) - expected) < 1e-9);
}

TEST(ShippingCostModel_FitRecoversKnownCoefficients) {
    const double trueB0 = 42.0;
    const double trueB1 = 1.35;
    const double trueB2 = 3.1;
    auto trueCost = [&](double d, double w) { return trueB0 + trueB1 * d + trueB2 * w; };
    // Noiseless data -> OLS recovers coefficients exactly.
    std::vector<std::pair<double, double>> distanceWeight = {
        {10.0, 100.0}, {20.0, 60.0}, {15.0, 200.0}, {50.0, 30.0}, {5.0, 150.0}, {35.0, 90.0}, {80.0, 10.0},
    };
    std::vector<ShippingCostModel::ShipmentRecord> records;
    for (const auto& [d, w] : distanceWeight) {
        records.push_back(ShippingCostModel::ShipmentRecord{d, w, trueCost(d, w)});
    }
    ShippingCostModel fitted = ShippingCostModel::fit(records);
    CHECK(std::abs(fitted.b0() - trueB0) < 1e-6);
    CHECK(std::abs(fitted.b1() - trueB1) < 1e-6);
    CHECK(std::abs(fitted.b2() - trueB2) < 1e-6);
}

TEST(Feasibility_RespectsCapacity) {
    ShippingCostModel model(0.0, 0.0, 0.0);
    std::vector<Order> orders = {Order(1, 0.0, 0.0, 4.0), Order(2, 1.0, 0.0, 5.0)};
    std::vector<SourcePlant> sources = {makeSource(1, 0.0, 0.0, 10.0, 1.0)};
    LandedCostProblem problem(orders, sources, model);
    LandedCostSolution solution;
    solution.assignments = {Assignment{1, 1}, Assignment{2, 1}};
    problem.validate(solution);
    CHECK(solution.feasible); // 4+5=9<=10
}

TEST(Feasibility_DetectsCapacityViolation) {
    ShippingCostModel model(0.0, 0.0, 0.0);
    std::vector<Order> orders = {Order(1, 0.0, 0.0, 6.0), Order(2, 1.0, 0.0, 7.0)};
    std::vector<SourcePlant> sources = {makeSource(1, 0.0, 0.0, 10.0, 1.0)};
    LandedCostProblem problem(orders, sources, model);
    LandedCostSolution solution;
    solution.assignments = {Assignment{1, 1}, Assignment{2, 1}};
    problem.validate(solution);
    CHECK(!solution.feasible); // 6+7=13>10
}

TEST(Feasibility_DetectsMissingOrder) {
    ShippingCostModel model(0.0, 0.0, 0.0);
    std::vector<Order> orders = {Order(1, 0.0, 0.0, 1.0), Order(2, 1.0, 0.0, 1.0)};
    std::vector<SourcePlant> sources = {makeSource(1, 0.0, 0.0, 10.0, 1.0)};
    LandedCostProblem problem(orders, sources, model);
    LandedCostSolution solution;
    solution.assignments = {Assignment{1, 1}}; // order 2 unassigned
    problem.validate(solution);
    CHECK(!solution.feasible);
}

TEST(Greedy_ProducesFeasibleSolutionWhenCapacityAllows) {
    ShippingCostModel model(10.0, 0.5, 0.1);
    std::vector<Order> orders = {
        Order(1, 0.0, 0.0, 3.0), Order(2, 1.0, 0.0, 4.0),
        Order(3, 50.0, 0.0, 3.0), Order(4, 51.0, 0.0, 4.0),
    };
    std::vector<SourcePlant> sources = {
        makeSource(1, 0.0, 0.0, 10.0, 2.0), makeSource(2, 50.0, 0.0, 10.0, 2.0),
    };
    LandedCostProblem problem(orders, sources, model);
    GreedyLowestCostSolver solver;
    LandedCostSolution solution = solver.solve(problem);
    CHECK(solution.feasible);
    CHECK(solution.assignments.size() == orders.size());
}

TEST(BranchAndBound_NeverWorseThanGreedy) {
    // Capacity-crunch case: sources tie on shipping cost; source 1 is
    // cheaper but capped at 10. Orders weigh 6,5,5 -- greedy seats the 6
    // first (leaving 4, too little for a 5); optimal pairs the two 5s.
    ShippingCostModel model(/*b0=*/50.0, /*b1=*/0.8, /*b2=*/2.5);
    std::vector<Order> orders = {
        Order(1, 5.0, 5.0, 6.0), Order(2, 5.0, 5.0, 5.0), Order(3, 5.0, 5.0, 5.0),
    };
    std::vector<SourcePlant> sources = {
        makeSource(1, 5.0, 5.0, 10.0, 1.0), makeSource(2, 5.0, 5.0, 100.0, 5.0),
    };
    LandedCostProblem problem(orders, sources, model);
    GreedyLowestCostSolver greedy;
    BranchAndBoundSolver exact;
    LandedCostSolution greedySolution = greedy.solve(problem);
    LandedCostSolution exactSolution = exact.solve(problem);
    CHECK(greedySolution.feasible);
    CHECK(exactSolution.feasible);
    CHECK(exactSolution.totalCost <= greedySolution.totalCost + 1e-9);
    CHECK(exactSolution.totalCost < greedySolution.totalCost - 1e-6); // strictly better
}

TEST(EdgeCase_ZeroOrders) {
    ShippingCostModel model(10.0, 0.5, 0.1);
    std::vector<Order> orders;
    std::vector<SourcePlant> sources = {makeSource(1, 0.0, 0.0, 10.0, 1.0)};
    LandedCostProblem problem(orders, sources, model);
    GreedyLowestCostSolver greedy;
    LandedCostSolution greedySolution = greedy.solve(problem);
    BranchAndBoundSolver exact;
    LandedCostSolution exactSolution = exact.solve(problem);
    CHECK(greedySolution.feasible);
    CHECK(greedySolution.assignments.empty());
    CHECK(greedySolution.totalCost == 0.0);
    CHECK(exactSolution.feasible);
    CHECK(exactSolution.assignments.empty());
    CHECK(exactSolution.totalCost == 0.0);
}

TEST(EdgeCase_InfeasibleWhenTotalWeightExceedsAllCapacity) {
    ShippingCostModel model(10.0, 0.5, 0.1);
    std::vector<Order> orders = {Order(1, 0.0, 0.0, 20.0)};
    std::vector<SourcePlant> sources = {makeSource(1, 0.0, 0.0, 10.0, 1.0)};
    LandedCostProblem problem(orders, sources, model);
    GreedyLowestCostSolver greedy;
    BranchAndBoundSolver exact;
    CHECK(!greedy.solve(problem).feasible);
    CHECK(!exact.solve(problem).feasible);
}
