#ifndef SOLUTION_EULER_CIRCUIT_H
#define SOLUTION_EULER_CIRCUIT_H

#include <model/Solution.h>
#include <vector>

enum class SolutionArcKind { Service, Traversal, Deposit };

struct OrderedSolutionArc {
    int id;
    int from;
    int to;
    SolutionArcKind kind;
};

// Expands the positive multiplicities of X, Y and the deposit connectors and
// returns the single ordered Euler circuit represented by the solution.
class SolutionEulerCircuit {
public:
    static std::vector<OrderedSolutionArc> build(const Solution& solution);
};

#endif
