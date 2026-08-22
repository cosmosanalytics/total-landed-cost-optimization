"""LandedCostSolver: strategy interface for sourcing/routing solvers."""

from abc import ABC, abstractmethod

from .problem import LandedCostProblem, LandedCostSolution


class LandedCostSolver(ABC):
    """So main.py and the tests can swap heuristic/exact/production
    backends without caring which one they're driving.
    """

    @abstractmethod
    def solve(self, problem: LandedCostProblem) -> LandedCostSolution:
        ...

    @abstractmethod
    def name(self) -> str:
        ...
