#pragma once

#include <string>

namespace landedcost {

// A candidate source plant (production or procurement site). Sources have
// no fixed "opening" cost -- unlike a facility-location hub, every source
// is always available, just capacity-limited on total weight it can
// supply across all orders assigned to it.
class SourcePlant {
public:
    SourcePlant(int id, std::string name, double x, double y, double capacity, double unitCost)
        : id_(id), name_(std::move(name)), x_(x), y_(y), capacity_(capacity),
          unitCost_(unitCost) {}

    int id() const { return id_; }
    const std::string& name() const { return name_; }
    double x() const { return x_; }
    double y() const { return y_; }
    double capacity() const { return capacity_; }
    double unitCost() const { return unitCost_; }

private:
    int id_;
    std::string name_;
    double x_;
    double y_;
    double capacity_;
    double unitCost_;
};

} // namespace landedcost
