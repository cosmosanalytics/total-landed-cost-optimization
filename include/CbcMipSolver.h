#pragma once

// CbcMipSolver -- production-scale path using COIN-OR CBC's C++ API.
// BranchAndBoundSolver.h is the zero-dependency exact solver used by
// default; this documents how the same model maps onto CBC (the solver
// the Python/PuLP version of this project actually uses via COIN-OR/CBC).
//
// Compiled only when LANDEDCOST_USE_CBC is defined (CMakeLists.txt's
// USE_CBC option), since it needs the COIN-OR CBC dev libraries:
//   sudo apt-get install coinor-libcbc-dev coinor-libclp-dev \
//                         coinor-libosi-dev coinor-libcoinutils-dev
//   cmake -DUSE_CBC=ON -B build && cmake --build build

#ifdef LANDEDCOST_USE_CBC

#include <algorithm>
#include <limits>

#include <CbcModel.hpp>
#include <CoinPackedMatrix.hpp>
#include <OsiClpSolverInterface.hpp>

#include "LandedCostSolver.h"

namespace landedcost {

// Solves LandedCostProblem as the capacitated transportation/assignment
// MIP described in the README, via CBC's Open Solver Interface (OSI).
// Variables: x_ij in {0,1} (order i served by source j). Unlike Network
// Optimization 3's facility-location MIP, there are no y_j "open" binary
// variables -- sources carry no fixed cost, so the capacity constraint is
// a plain sum_i weight_i*x_ij <= capacity_j rather than a big-M-style
// coupling constraint.
class CbcMipSolver : public LandedCostSolver {
public:
    LandedCostSolution solve(const LandedCostProblem& problem) override {
        const auto& orders = problem.orders();
        const auto& sources = problem.sources();
        const int n = static_cast<int>(orders.size());
        const int m = static_cast<int>(sources.size());

        const int numVars = n * m;
        auto xIndex = [m](int i, int j) { return i * m + j; };

        OsiClpSolverInterface solver;

        std::vector<double> objective(numVars, 0.0);
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < m; ++j) {
                objective[xIndex(i, j)] = problem.landedCost(orders[i], sources[j]);
            }
        }

        std::vector<double> colLower(numVars, 0.0);
        std::vector<double> colUpper(numVars, 1.0);

        CoinPackedMatrix matrix(false, 0, 0);
        matrix.setDimensions(0, numVars);

        std::vector<double> rowLower;
        std::vector<double> rowUpper;

        // sum_j x_ij = 1 for every order i.
        for (int i = 0; i < n; ++i) {
            CoinPackedVector row;
            for (int j = 0; j < m; ++j) row.insert(xIndex(i, j), 1.0);
            matrix.appendRow(row);
            rowLower.push_back(1.0);
            rowUpper.push_back(1.0);
        }

        // sum_i weight_i*x_ij <= capacity_j for every source j.
        for (int j = 0; j < m; ++j) {
            CoinPackedVector row;
            for (int i = 0; i < n; ++i) row.insert(xIndex(i, j), orders[i].weight());
            matrix.appendRow(row);
            rowLower.push_back(-COIN_DBL_MAX);
            rowUpper.push_back(sources[j].capacity());
        }

        solver.loadProblem(matrix, colLower.data(), colUpper.data(),
                            objective.data(), rowLower.data(), rowUpper.data());
        for (int v = 0; v < numVars; ++v) solver.setInteger(v);

        CbcModel model(solver);
        model.setLogLevel(0);
        model.branchAndBound();

        LandedCostSolution result;
        const double* sol = model.solver()->getColSolution();
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < m; ++j) {
                if (sol[xIndex(i, j)] > 0.5) {
                    result.assignments.push_back(Assignment{orders[i].id(), sources[j].id()});
                }
            }
        }
        problem.validate(result);
        return result;
    }

    std::string name() const override { return "Cbc-MIP"; }
};

} // namespace landedcost

#endif // LANDEDCOST_USE_CBC
