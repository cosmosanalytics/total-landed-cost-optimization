#include "ShippingCostModel.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace landedcost {

namespace {

// Solves the 3x3 linear system A*x = b in place via Gaussian elimination
// with partial pivoting. A is row-major 3x3, b has 3 entries.
std::array<double, 3> solve3x3(std::array<std::array<double, 3>, 3> a, std::array<double, 3> b) {
    constexpr int n = 3;
    for (int col = 0; col < n; ++col) {
        int pivotRow = col;
        double pivotMag = std::abs(a[col][col]);
        for (int row = col + 1; row < n; ++row) {
            if (std::abs(a[row][col]) > pivotMag) {
                pivotMag = std::abs(a[row][col]);
                pivotRow = row;
            }
        }
        if (pivotMag < 1e-12) {
            throw std::runtime_error(
                "ShippingCostModel::fit: normal-equations matrix is singular "
                "(records are collinear -- vary distance and weight independently)");
        }
        if (pivotRow != col) {
            std::swap(a[pivotRow], a[col]);
            std::swap(b[pivotRow], b[col]);
        }
        for (int row = col + 1; row < n; ++row) {
            const double factor = a[row][col] / a[col][col];
            for (int k = col; k < n; ++k) a[row][k] -= factor * a[col][k];
            b[row] -= factor * b[col];
        }
    }

    std::array<double, 3> x{};
    for (int row = n - 1; row >= 0; --row) {
        double sum = b[row];
        for (int k = row + 1; k < n; ++k) sum -= a[row][k] * x[k];
        x[row] = sum / a[row][row];
    }
    return x;
}

} // namespace

ShippingCostModel ShippingCostModel::fit(const std::vector<ShipmentRecord>& records) {
    if (records.size() < 3) {
        throw std::invalid_argument(
            "ShippingCostModel::fit requires at least 3 shipment records");
    }

    // Design matrix columns are [1, distance, weight]; build the 3x3
    // normal-equations matrix X^T X and right-hand side X^T y directly
    // (cheaper than materializing X for a 3-parameter model).
    std::array<std::array<double, 3>, 3> xtx{};
    std::array<double, 3> xty{};

    for (const ShipmentRecord& r : records) {
        const std::array<double, 3> row = {1.0, r.distance, r.weight};
        for (std::size_t i = 0; i < 3; ++i) {
            xty[i] += row[i] * r.actualCost;
            for (std::size_t j = 0; j < 3; ++j) {
                xtx[i][j] += row[i] * row[j];
            }
        }
    }

    const std::array<double, 3> beta = solve3x3(xtx, xty);
    return ShippingCostModel(beta[0], beta[1], beta[2]);
}

} // namespace landedcost
