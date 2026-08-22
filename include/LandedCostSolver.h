#pragma once

#include <string>

#include "LandedCostProblem.h"

namespace landedcost {

// Strategy interface for sourcing/routing solvers, so main.cpp and the
// tests can swap heuristic/exact/production backends without caring which
// one they're driving.
class LandedCostSolver {
public:
    virtual ~LandedCostSolver() = default;
    virtual LandedCostSolution solve(const LandedCostProblem& problem) = 0;
    virtual std::string name() const = 0;
};

} // namespace landedcost
