#include "BranchAndBoundSolver.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "GreedyLowestCostSolver.h"

namespace landedcost {

namespace {

struct SearchState {
    const LandedCostProblem* problem;
    const std::vector<Order>* orders;
    const std::vector<SourcePlant>* sources;
    std::vector<double> cheapestPossibleCost; // per order index, min landed cost over all sources
    std::vector<double> suffixLowerBound;     // suffixLowerBound[i] = sum of cheapestPossibleCost[i..n-1]

    std::vector<int> assignment;   // assignment[i] = source index chosen for order i
    std::vector<double> remaining; // remaining[j] = capacity left at source j

    std::vector<int> bestAssignment;
    double bestCost = std::numeric_limits<double>::max();
};

void search(SearchState& s, int orderIdx, double runningCost) {
    const std::size_t n = s.orders->size();
    if (static_cast<std::size_t>(orderIdx) == n) {
        if (runningCost < s.bestCost) {
            s.bestCost = runningCost;
            s.bestAssignment = s.assignment;
        }
        return;
    }

    if (runningCost + s.suffixLowerBound[orderIdx] >= s.bestCost) {
        return; // even the most optimistic completion can't beat the incumbent
    }

    const Order& order = (*s.orders)[orderIdx];

    // Try cheaper sources first so a good incumbent is found early, which
    // makes the lower-bound prune above effective sooner.
    std::vector<int> sourceOrder(s.sources->size());
    for (std::size_t j = 0; j < sourceOrder.size(); ++j) sourceOrder[j] = static_cast<int>(j);
    std::sort(sourceOrder.begin(), sourceOrder.end(), [&](int a, int b) {
        return s.problem->landedCost(order, (*s.sources)[a]) <
               s.problem->landedCost(order, (*s.sources)[b]);
    });

    for (int j : sourceOrder) {
        const SourcePlant& source = (*s.sources)[j];
        if (s.remaining[j] + 1e-9 < order.weight()) continue;

        const double addedCost = s.problem->landedCost(order, source);

        s.assignment[orderIdx] = j;
        s.remaining[j] -= order.weight();

        search(s, orderIdx + 1, runningCost + addedCost);

        s.remaining[j] += order.weight();
    }
    s.assignment[orderIdx] = -1;
}

} // namespace

LandedCostSolution BranchAndBoundSolver::solve(const LandedCostProblem& problem) {
    const auto& orders = problem.orders();
    const auto& sources = problem.sources();

    LandedCostSolution result;
    if (orders.empty()) {
        result.feasible = true;
        result.totalCost = 0.0;
        return result;
    }

    SearchState s;
    s.problem = &problem;
    s.orders = &orders;
    s.sources = &sources;
    s.assignment.assign(orders.size(), -1);
    s.remaining.resize(sources.size());
    for (std::size_t j = 0; j < sources.size(); ++j) s.remaining[j] = sources[j].capacity();

    // Admissible lower bound: cheapest single-source landed cost per
    // order, ignoring capacity.
    s.cheapestPossibleCost.resize(orders.size());
    for (std::size_t i = 0; i < orders.size(); ++i) {
        double best = std::numeric_limits<double>::max();
        for (const auto& source : sources) {
            best = std::min(best, problem.landedCost(orders[i], source));
        }
        s.cheapestPossibleCost[i] = sources.empty() ? 0.0 : best;
    }
    s.suffixLowerBound.assign(orders.size() + 1, 0.0);
    for (std::size_t i = orders.size(); i-- > 0;) {
        s.suffixLowerBound[i] = s.suffixLowerBound[i + 1] + s.cheapestPossibleCost[i];
    }

    // Seed the incumbent with the greedy heuristic's result so pruning is
    // effective from the very first branch instead of only after the
    // search stumbles onto a decent solution by luck.
    GreedyLowestCostSolver greedy;
    LandedCostSolution greedySolution = greedy.solve(problem);
    if (greedySolution.feasible) s.bestCost = greedySolution.totalCost;

    if (!sources.empty()) {
        search(s, 0, 0.0);
    }

    if (s.bestAssignment.empty() && !greedySolution.feasible) {
        // No feasible assignment exists at all (e.g. total weight exceeds
        // total capacity) -- report the infeasible greedy attempt so the
        // caller can see how far off it was, same as the sibling projects.
        return greedySolution;
    }
    if (s.bestAssignment.empty()) {
        return greedySolution; // greedy already found the (only) feasible answer
    }

    result.assignments.reserve(orders.size());
    for (std::size_t i = 0; i < orders.size(); ++i) {
        const int sourceIdx = s.bestAssignment[i];
        result.assignments.push_back(Assignment{orders[i].id(), sources[sourceIdx].id()});
    }

    problem.validate(result);
    return result;
}

} // namespace landedcost
