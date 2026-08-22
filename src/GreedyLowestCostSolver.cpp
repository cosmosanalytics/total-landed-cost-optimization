#include "GreedyLowestCostSolver.h"

#include <algorithm>
#include <limits>
#include <unordered_map>

namespace landedcost {

LandedCostSolution GreedyLowestCostSolver::solve(const LandedCostProblem& problem) {
    const auto& orders = problem.orders();
    const auto& sources = problem.sources();

    // Heaviest-first: place the orders that are hardest to fit while
    // capacity is still plentiful.
    std::vector<int> order(orders.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return orders[a].weight() > orders[b].weight();
    });

    std::unordered_map<int, double> remainingCapacity;
    for (const auto& source : sources) remainingCapacity[source.id()] = source.capacity();

    LandedCostSolution solution;
    solution.assignments.reserve(orders.size());

    for (int idx : order) {
        const Order& o = orders[idx];

        // Prefer the lowest-landed-cost source with enough remaining
        // capacity.
        const SourcePlant* chosen = nullptr;
        double bestCost = std::numeric_limits<double>::max();
        for (const auto& source : sources) {
            if (remainingCapacity[source.id()] + 1e-9 < o.weight()) continue;
            const double c = problem.landedCost(o, source);
            if (c < bestCost) {
                bestCost = c;
                chosen = &source;
            }
        }
        // Nothing has room: fall back to the lowest-landed-cost source
        // overall so the solution stays complete; validate() will mark it
        // infeasible.
        if (!chosen) {
            for (const auto& source : sources) {
                const double c = problem.landedCost(o, source);
                if (c < bestCost) {
                    bestCost = c;
                    chosen = &source;
                }
            }
        }
        if (!chosen) continue; // no sources at all -- nothing we can do

        solution.assignments.push_back(Assignment{o.id(), chosen->id()});
        remainingCapacity[chosen->id()] -= o.weight();
    }

    problem.validate(solution);
    return solution;
}

} // namespace landedcost
