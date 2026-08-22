#pragma once

#include <vector>

namespace landedcost {

// Linear shipping-cost regression: predicted cost as a function of
// distance and shipment weight,
//
//   cost = b0 + b1*distance + b2*weight
//
// (an intercept/base-handling-fee term plus a per-mile rate and a
// per-pound rate). This is the "shipping-cost regression based on
// distance and weight" from the resume bullet: fit it once against
// historical shipment records, then reuse the fitted model to price
// every candidate order/source pairing in the optimization.
//
// A three-parameter model was chosen over a four-parameter one with a
// distance*weight interaction term: the extra term makes the normal-
// equations matrix noticeably closer to singular on the kind of small,
// unreplicated synthetic fixtures this project tests against, which is a
// numerical-robustness cost not worth paying for a demo-scale model. The
// intercept + two linear rates already captures the "regression on
// distance and weight" story faithfully.
class ShippingCostModel {
public:
    // One historical (or synthetic) shipment used to fit the model.
    struct ShipmentRecord {
        double distance;
        double weight;
        double actualCost;
    };

    ShippingCostModel(double b0, double b1, double b2) : b0_(b0), b1_(b1), b2_(b2) {}

    double b0() const { return b0_; }
    double b1() const { return b1_; }
    double b2() const { return b2_; }

    // Predicted shipping cost for one shipment.
    double estimate(double distance, double weight) const {
        return b0_ + b1_ * distance + b2_ * weight;
    }

    // Ordinary least squares fit of b0/b1/b2 against a set of shipment
    // records, via the normal equations (X^T X) b = X^T y solved with
    // hand-written Gaussian elimination (partial pivoting, no external
    // linear algebra library). Requires at least 3 records that are not
    // all collinear (varying distance and weight independently, as the
    // fixtures in this project's tests do).
    static ShippingCostModel fit(const std::vector<ShipmentRecord>& records);

private:
    double b0_;
    double b1_;
    double b2_;
};

} // namespace landedcost
