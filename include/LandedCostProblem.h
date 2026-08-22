#pragma once

#include <vector>

#include "Order.h"
#include "ShippingCostModel.h"
#include "SourcePlant.h"

namespace landedcost {

// One order routed to one source plant.
struct Assignment {
    int orderId;
    int sourceId;
};

// A candidate sourcing/routing plan produced by a solver. Mirrors the
// Territory/ClusteringSolution shape from Network Optimization 3: solvers
// build one of these, then hand it to LandedCostProblem::validate() to
// check feasibility and price it.
struct LandedCostSolution {
    bool feasible = true;
    double totalCost = 0.0;
    std::vector<Assignment> assignments; // one entry per order
};

// A capacitated transportation/assignment problem: every order must be
// assigned to exactly one candidate source plant, minimizing total landed
// cost -- unit production/procurement cost plus a regression-estimated
// shipping cost -- subject to each source's total supplied-weight
// capacity. Unlike Network Optimization 3's facility-location model,
// sources carry no fixed opening cost: they are always available, only
// capacity-limited.
class LandedCostProblem {
public:
    LandedCostProblem(std::vector<Order> orders, std::vector<SourcePlant> sources,
                       ShippingCostModel shippingModel);

    const std::vector<Order>& orders() const { return orders_; }
    const std::vector<SourcePlant>& sources() const { return sources_; }
    const ShippingCostModel& shippingModel() const { return shippingModel_; }

    // Straight-line distance between an order's destination and a source.
    double distance(const Order& order, const SourcePlant& source) const;

    // Total landed cost of serving one order from one source: unit cost
    // times weight, plus the regression-estimated shipping cost for that
    // distance and weight.
    double landedCost(const Order& order, const SourcePlant& source) const;

    const SourcePlant* findSource(int sourceId) const;
    const Order* findOrder(int orderId) const;

    // Recomputes feasibility and totalCost for a solution built by a
    // solver: every order must appear exactly once, referencing a known
    // source; per-source total assigned weight must not exceed capacity
    // (+1e-9 tolerance); cost is the sum of landedCost over all
    // assignments if feasible, else 0.
    void validate(LandedCostSolution& solution) const;

private:
    std::vector<Order> orders_;
    std::vector<SourcePlant> sources_;
    ShippingCostModel shippingModel_;
};

} // namespace landedcost
