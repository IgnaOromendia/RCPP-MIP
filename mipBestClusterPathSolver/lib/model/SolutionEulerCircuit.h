#ifndef SOLUTION_EULER_CIRCUIT_H
#define SOLUTION_EULER_CIRCUIT_H

#include <model/Solution.h>
#include <cstddef>
#include <vector>

enum class SolutionArcKind { Service, Traversal, Deposit };

struct ClusterSolutionArc {
    int id;
    int from;
    int to;
    SolutionArcKind kind;
    long long remaining_passes;

    ClusterSolutionArc(const ArcValue<long long>& value, const SolutionArcKind& kind)
        : id(value.id), from(value.from), to(value.to), kind(kind),
          remaining_passes(value.value) {}
};

// Expands the positive multiplicities of X, Y and the deposit connectors and
// returns the ordered depot circuit. Disconnected traversal-only subtours are
// ignored, but every serviced arc must belong to that circuit.
class SolutionEulerCircuit {
public:
    SolutionEulerCircuit(int node_count, const Solution& solution);

    vector<ClusterSolutionArc> circuit();

private:
    vector<vector<int>> segment_adj;
    vector<ClusterSolutionArc> pending;
    long long total = 0;
    int deposit;

    void add_pending_segments(const vector<ArcValue<long long>>& values, SolutionArcKind kind);
};

#endif
