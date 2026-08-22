#pragma once

#include "LandedCostSolver.h"

namespace landedcost {

// Fast heuristic: process orders heaviest-first (same rationale as
// Network Optimization 2/3's largest-first heuristics -- place the
// hardest-to-fit demand while capacity is still plentiful), assigning
// each order to the source with the lowest landed cost that still has
// enough remaining capacity. Falls back to the lowest-landed-cost source
// overall (ignoring capacity) if nothing fits, so the caller always gets
// a complete solution to inspect -- LandedCostProblem::validate() is what
// actually flags infeasibility.
class GreedyLowestCostSolver : public LandedCostSolver {
public:
    LandedCostSolution solve(const LandedCostProblem& problem) override;
    std::string name() const override { return "Greedy-LowestCost"; }
};

} // namespace landedcost
