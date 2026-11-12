#ifndef SOLUTION_EULER_CIRCUIT_H
#define SOLUTION_EULER_CIRCUIT_H

#include <model/Solution.h>
#include <cstddef>
#include <vector>

enum class SolutionArcKind { Service, Traversal, Deposit, Provisional };

struct ClusterSolutionArc {
    int id;
    int from;
    int to;
    SolutionArcKind kind;
    long long remaining_passes;

    ClusterSolutionArc(const ArcValue<long long>& value, const SolutionArcKind& kind)
        : id(value.id), from(value.from), to(value.to), kind(kind),
          remaining_passes(value.value) {}

    ClusterSolutionArc(int id, int from, int to, SolutionArcKind kind, long long count)
        : id(id), from(from), to(to), kind(kind), remaining_passes(count) {}
};

// Expands positive X, Y, deposit connectors and optional extra segments into
// the ordered depot circuit. Every positive occurrence must belong to that
// circuit; disconnected traversal-only subtours are rejected.
class SolutionEulerCircuit {
public:
    SolutionEulerCircuit(int node_count, const Solution& solution,
        const vector<ClusterSolutionArc>& extra_segments = {});

    vector<ClusterSolutionArc> circuit();

private:
    vector<vector<int>> segment_adj;
    vector<ClusterSolutionArc> pending;
    long long total = 0;
    int deposit;

    void add_pending_segments(const vector<ArcValue<long long>>& values, SolutionArcKind kind);
    void add_pending_segment(const ClusterSolutionArc& segment);
};

#endif
