#include "LandedCostProblem.h"

#include <cmath>
#include <unordered_map>

namespace landedcost {

LandedCostProblem::LandedCostProblem(std::vector<Order> orders, std::vector<SourcePlant> sources,
                                      ShippingCostModel shippingModel)
    : orders_(std::move(orders)), sources_(std::move(sources)),
      shippingModel_(std::move(shippingModel)) {}

double LandedCostProblem::distance(const Order& order, const SourcePlant& source) const {
    const double dx = order.x() - source.x();
    const double dy = order.y() - source.y();
    return std::sqrt(dx * dx + dy * dy);
}

double LandedCostProblem::landedCost(const Order& order, const SourcePlant& source) const {
    const double productionCost = source.unitCost() * order.weight();
    const double shippingCost = shippingModel_.estimate(distance(order, source), order.weight());
    return productionCost + shippingCost;
}

const SourcePlant* LandedCostProblem::findSource(int sourceId) const {
    for (const auto& s : sources_) {
        if (s.id() == sourceId) return &s;
    }
    return nullptr;
}

const Order* LandedCostProblem::findOrder(int orderId) const {
    for (const auto& o : orders_) {
        if (o.id() == orderId) return &o;
    }
    return nullptr;
}

void LandedCostProblem::validate(LandedCostSolution& solution) const {
    std::unordered_map<int, int> timesSeen;
    for (const Order& o : orders_) timesSeen[o.id()] = 0;

    std::unordered_map<int, double> usedCapacity;
    for (const SourcePlant& s : sources_) usedCapacity[s.id()] = 0.0;

    bool feasible = true;
    double cost = 0.0;

    for (const Assignment& a : solution.assignments) {
        const Order* order = findOrder(a.orderId);
        const SourcePlant* source = findSource(a.sourceId);
        if (!order || !source) {
            feasible = false;
            continue;
        }
        ++timesSeen[order->id()];
        usedCapacity[source->id()] += order->weight();
        cost += landedCost(*order, *source);
    }

    for (const auto& [orderId, count] : timesSeen) {
        (void)orderId;
        if (count != 1) feasible = false; // every order exactly once
    }
    for (const SourcePlant& s : sources_) {
        if (usedCapacity[s.id()] > s.capacity() + 1e-9) feasible = false;
    }

    solution.feasible = feasible;
    solution.totalCost = feasible ? cost : 0.0;
}

} // namespace landedcost
