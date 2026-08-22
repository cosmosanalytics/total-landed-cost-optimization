"""ShippingCostModel: linear shipping-cost regression.

Predicted cost as a function of distance and shipment weight,

    cost = b0 + b1*distance + b2*weight

(an intercept/base-handling-fee term plus a per-mile rate and a per-pound
rate). Fit once against historical shipment records, then reuse the fitted
model to price every candidate order/source pairing in the optimization.

A three-parameter model was chosen over a four-parameter one with a
distance*weight interaction term: the extra term makes the normal-equations
matrix noticeably closer to singular on the kind of small, unreplicated
synthetic fixtures this project tests against, which is a numerical-
robustness cost not worth paying for a demo-scale model.
"""

from dataclasses import dataclass
from typing import List, NamedTuple


class ShipmentRecord(NamedTuple):
    """One historical (or synthetic) shipment used to fit the model."""

    distance: float
    weight: float
    actual_cost: float


def _solve_3x3(a: List[List[float]], b: List[float]) -> List[float]:
    """Solves the 3x3 linear system A*x = b via Gaussian elimination with
    partial pivoting. Mirrors ShippingCostModel.cpp's solve3x3 exactly.
    """
    n = 3
    a = [row[:] for row in a]
    b = b[:]
    for col in range(n):
        pivot_row = col
        pivot_mag = abs(a[col][col])
        for row in range(col + 1, n):
            if abs(a[row][col]) > pivot_mag:
                pivot_mag = abs(a[row][col])
                pivot_row = row
        if pivot_mag < 1e-12:
            raise ValueError(
                "ShippingCostModel.fit: normal-equations matrix is singular "
                "(records are collinear -- vary distance and weight independently)"
            )
        if pivot_row != col:
            a[pivot_row], a[col] = a[col], a[pivot_row]
            b[pivot_row], b[col] = b[col], b[pivot_row]
        for row in range(col + 1, n):
            factor = a[row][col] / a[col][col]
            for k in range(col, n):
                a[row][k] -= factor * a[col][k]
            b[row] -= factor * b[col]

    x = [0.0] * n
    for row in range(n - 1, -1, -1):
        s = b[row]
        for k in range(row + 1, n):
            s -= a[row][k] * x[k]
        x[row] = s / a[row][row]
    return x


@dataclass(frozen=True)
class ShippingCostModel:
    b0: float
    b1: float
    b2: float

    def estimate(self, distance: float, weight: float) -> float:
        """Predicted shipping cost for one shipment."""
        return self.b0 + self.b1 * distance + self.b2 * weight

    @staticmethod
    def fit(records: List[ShipmentRecord]) -> "ShippingCostModel":
        """Ordinary least squares fit of b0/b1/b2 against a set of shipment
        records, via the normal equations (X^T X) b = X^T y solved with
        hand-written Gaussian elimination (partial pivoting, no external
        linear algebra library). Requires at least 3 records that are not
        all collinear (varying distance and weight independently).
        """
        if len(records) < 3:
            raise ValueError("ShippingCostModel.fit requires at least 3 shipment records")

        xtx = [[0.0] * 3 for _ in range(3)]
        xty = [0.0] * 3

        for r in records:
            row = (1.0, r.distance, r.weight)
            for i in range(3):
                xty[i] += row[i] * r.actual_cost
                for j in range(3):
                    xtx[i][j] += row[i] * row[j]

        beta = _solve_3x3(xtx, xty)
        return ShippingCostModel(beta[0], beta[1], beta[2])
