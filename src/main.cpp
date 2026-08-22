#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "BranchAndBoundSolver.h"
#include "GreedyLowestCostSolver.h"
#include "LandedCostProblem.h"

using namespace landedcost;

namespace {

void printSolution(const std::string& solverName, const LandedCostProblem& problem,
                    const LandedCostSolution& solution) {
    std::cout << "\n--- " << solverName << " ---\n";
    std::cout << "Feasible: " << (solution.feasible ? "yes" : "no") << "\n";
    std::cout << "Total landed cost: $" << std::fixed << std::setprecision(2)
              << solution.totalCost << "\n";

    // Group assignments by source for a readable per-source breakdown.
    std::unordered_map<int, std::vector<int>> ordersBySource;
    std::unordered_map<int, double> weightBySource;
    for (const Assignment& a : solution.assignments) {
        ordersBySource[a.sourceId].push_back(a.orderId);
        const Order* order = problem.findOrder(a.orderId);
        if (order) weightBySource[a.sourceId] += order->weight();
    }

    for (const SourcePlant& source : problem.sources()) {
        auto it = ordersBySource.find(source.id());
        if (it == ordersBySource.end()) continue;
        std::cout << "  " << source.name() << " (load " << weightBySource[source.id()] << " / "
                  << source.capacity() << " capacity): orders {";
        const std::vector<int>& ids = it->second;
        for (std::size_t i = 0; i < ids.size(); ++i) {
            std::cout << ids[i] << (i + 1 < ids.size() ? ", " : "");
        }
        std::cout << "}\n";
    }
}

} // namespace

int main() {
    // Shipping-cost regression: cost = b0 + b1*distance + b2*weight,
    // i.e. a base handling fee plus a per-mile rate and a per-pound rate.
    // In practice this model would be fit with ShippingCostModel::fit()
    // against historical shipment records; here we use plausible
    // hand-picked coefficients for the demo.
    ShippingCostModel shippingModel(/*b0=*/50.0, /*b1=*/0.8, /*b2=*/2.5);

    // Orders spread across two regions (destinations in projected miles),
    // weight in pounds.
    std::vector<Order> orders = {
        Order(1, 10.0, 10.0, 120.0),  Order(2, 15.0, 8.0, 90.0),
        Order(3, 12.0, 20.0, 200.0),  Order(4, 8.0, 15.0, 60.0),
        Order(5, 300.0, 210.0, 150.0), Order(6, 310.0, 200.0, 80.0),
        Order(7, 295.0, 220.0, 250.0), Order(8, 305.0, 215.0, 100.0),
        Order(9, 150.0, 100.0, 175.0), Order(10, 20.0, 5.0, 130.0),
    };

    // Candidate source plants with differing unit cost, capacity, and
    // location.
    std::vector<SourcePlant> sources = {
        SourcePlant(1, "West Coast Plant", 0.0, 0.0, 500.0, 4.00),
        SourcePlant(2, "Midwest Plant", 150.0, 90.0, 450.0, 3.50),
        SourcePlant(3, "East Coast Plant", 320.0, 210.0, 500.0, 4.25),
        SourcePlant(4, "Overseas Supplier", 0.0, 400.0, 400.0, 2.75),
    };

    LandedCostProblem problem(orders, sources, shippingModel);

    std::cout << "Total Landed Cost Optimization (C++) \u2014 Sourcing/Routing Assignment MIP\n";
    std::cout << orders.size() << " orders, " << sources.size()
              << " candidate source plants\n";
    std::cout << "Shipping model: cost = " << shippingModel.b0() << " + " << shippingModel.b1()
              << "*distance + " << shippingModel.b2() << "*weight\n";

    GreedyLowestCostSolver greedy;
    printSolution(greedy.name(), problem, greedy.solve(problem));

    BranchAndBoundSolver exact;
    printSolution(exact.name() + " (exact)", problem, exact.solve(problem));

    return 0;
}
