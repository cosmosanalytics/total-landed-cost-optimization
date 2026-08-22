#pragma once

#include "LandedCostSolver.h"

namespace landedcost {

// Exact, dependency-free solver for the capacitated transportation /
// sourcing-assignment problem. Branches over which source serves each
// order, tracking per-source remaining capacity (no fixed-cost/opened-
// tracking needed since sources have no activation cost -- every source
// is always available). Prunes with an admissible lower bound: the
// cheapest possible landed cost across all sources, ignoring capacity,
// for every not-yet-assigned order (a valid lower bound since ignoring
// capacity can only underestimate the true cost).
//
// Exponential in the worst case, like any exact assignment solver, so it
// is meant for the small/medium instances a portfolio demo or a
// regression test uses -- CbcMipSolver.h is the path for production
// scale.
class BranchAndBoundSolver : public LandedCostSolver {
public:
    LandedCostSolution solve(const LandedCostProblem& problem) override;
    std::string name() const override { return "BranchAndBound-Exact"; }
};

} // namespace landedcost
