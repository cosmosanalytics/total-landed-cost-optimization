#pragma once

namespace landedcost {

// A customer order to be sourced: a destination location and a shipment
// weight. Coordinates are treated as a flat-plane approximation (e.g.
// projected miles/km) -- good enough for a sourcing/routing design model.
class Order {
public:
    Order(int id, double x, double y, double weight)
        : id_(id), x_(x), y_(y), weight_(weight) {}

    int id() const { return id_; }
    double x() const { return x_; }
    double y() const { return y_; }
    double weight() const { return weight_; }

private:
    int id_;
    double x_;
    double y_;
    double weight_;
};

} // namespace landedcost
