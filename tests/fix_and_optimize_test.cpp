#include "TestInstance.h"
#include "lib/heuristic/SelectionStrategy.h"
#include "lib/model/Solution.h"

struct SolveResult {
    bool has_solution = false;
    double objective = 0;
    Solution solution;

    double get_obj_value() const { return objective; }
    Solution extract_solution() const { return solution; }
};

class RCPPSolver {
public:
    RCPPSolver(const SuperGraph&, int) {}
    void generate_MIP() {}
    void set_time_objective() {}
    SolveResult solve(double = 0) { return {}; }
    SolveResult solve_neighborhood(const Solution&, const std::vector<EdgeKey>&, double) {
        return {};
    }
};

// FixAndOptimize includes RCPPSolver through this guard. The test supplies the
// minimal double above so selecting neighborhoods cannot construct a MIP.
#define RCPPSOLVER_H

// Keep production encapsulation unchanged. All dependencies have already been
// parsed, so this exposes only FixAndOptimize's private selector in this test.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "lib/heuristic/FixAndOptimize.h"
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// Compile the production implementation in this isolated unit. Its solver
// calls bind to the double above; the tests invoke only top_k_neighborhood().
#include "../mipSolver/src/heuristic/FixAndOptimize.cpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

SuperGraph make_graph(int nodes,
                      std::vector<InstanceEdge> edges,
                      std::vector<InstanceEdge> arcs) {
    Instance instance;
    instance.vehicles = 1;
    instance.nodes = nodes;
    instance.deposit_nodes = {0};
    instance.edges = std::move(edges);
    instance.arcs = std::move(arcs);
    return test_super_graph(instance);
}

Solution solution_with_traversals(const SuperGraph& graph,
                                  const std::vector<std::pair<int, long long>>& values) {
    Solution solution;
    for (const auto& [arc_id, value] : values) {
        const SuperArc& arc = *graph.super_arc_with_id(arc_id);
        solution.traversals.push_back({arc.id, arc.from, arc.to, 1, value});
    }
    return solution;
}

std::vector<EdgeKey> select(const SuperGraph& graph,
                            int reachability,
                            int k,
                            const Solution& solution) {
    FixAndOptimize heuristic(graph, 1, reachability);
    // Do not call solve(): this test must not generate the MIP model.
    return heuristic.top_k_neighborhood(k, solution);
}

bool contains(const std::vector<EdgeKey>& edges, const SuperArc& arc) {
    return std::find(edges.begin(), edges.end(), EdgeKey{arc.from, arc.to}) != edges.end();
}

std::set<EdgeKey> edge_set(const std::vector<EdgeKey>& edges) {
    return {edges.begin(), edges.end()};
}

void check_weight_cutoff_and_k_limit() {
    const auto graph = make_graph(
        6, {}, {{0, 1, 0, 100, 0}, {2, 3, 0, 50, 0}, {4, 5, 0, 24, 0}});
    const auto solution = solution_with_traversals(graph, {{0, 1}, {1, 1}, {2, 1}});

    const auto top_one = select(graph, 1, 1, solution);
    check(contains(top_one, *graph.super_arc_with_id(0)), "k=1 must select only the best arc");
    check(!contains(top_one, *graph.super_arc_with_id(1)), "k=1 selected a second arc");

    const auto cutoff = select(graph, 1, 3, solution);
    check(contains(cutoff, *graph.super_arc_with_id(0)), "Cutoff lost the best arc");
    check(contains(cutoff, *graph.super_arc_with_id(1)),
          "A candidate at exactly half the previous weight must be accepted");
    check(!contains(cutoff, *graph.super_arc_with_id(2)),
          "A candidate below half the previous weight must trigger the cutoff");
}

void check_candidate_boundaries() {
    const auto graph = make_graph(4, {}, {{0, 1, 0, 10, 0}, {2, 3, 0, 8, 0}});
    const auto solution = solution_with_traversals(graph, {{0, 1}, {1, 1}});
    const auto neighborhood = select(graph, 1, 10, solution);

    check(contains(neighborhood, *graph.super_arc_with_id(0)), "Missing first candidate");
    check(contains(neighborhood, *graph.super_arc_with_id(1)), "Missing second candidate");
    check(edge_set(neighborhood).size() == 2, "Selection added unexpected edges");
    check(select(graph, 1, 10, {}).empty(),
          "A solution without deadheading must have no top-k neighborhood");
}

void check_covered_center_uses_next_candidate() {
    const auto graph = make_graph(
        4, {}, {{0, 1, 0, 10, 0}, {1, 2, 0, 9, 0}, {2, 3, 0, 8, 0}});
    const auto solution = solution_with_traversals(graph, {{0, 10}, {1, 10}, {2, 10}});
    const auto neighborhood = select(graph, 3, 2, solution);

    check(contains(neighborhood, *graph.super_arc_with_id(0)), "Missing best center");
    check(contains(neighborhood, *graph.super_arc_with_id(1)),
          "The best center's BFS must cover the next physical arc");
    check(contains(neighborhood, *graph.super_arc_with_id(2)),
          "A covered second center must be replaced by the next candidate");
}

void check_paired_orientation_uses_next_candidate() {
    const auto graph = make_graph(4, {{0, 1, 0, 10, 0}}, {{2, 3, 0, 8, 0}});
    const auto& forward = *graph.super_arc_with_id(0);
    const auto& reverse = *graph.super_arc_with_id(1);
    const auto& directed = *graph.super_arc_with_id(2);
    check(forward.pair == reverse.id && reverse.pair == forward.id,
          "Fixture must contain a paired edge");

    const auto solution = solution_with_traversals(
        graph, {{forward.id, 10}, {reverse.id, 9}, {directed.id, 10}});
    const auto neighborhood = select(graph, 1, 2, solution);

    check(contains(neighborhood, forward), "Missing highest-weight orientation");
    check(!contains(neighborhood, reverse), "Paired orientation was selected twice");
    check(contains(neighborhood, directed),
          "Skipped paired orientation was not replaced by the next candidate");
}

void check_overlapping_neighborhoods_are_allowed() {
    const auto graph = make_graph(
        4, {}, {{0, 2, 0, 10, 0}, {1, 2, 0, 9, 0}, {2, 3, 0, 1, 0}});
    const auto solution = solution_with_traversals(graph, {{0, 10}, {1, 10}});
    const auto first = graph.edge_subset(graph.super_arc_with_id(0)->from, 3);
    const auto second = graph.edge_subset(graph.super_arc_with_id(1)->from, 3);
    const auto first_set = edge_set(first);
    const auto second_set = edge_set(second);
    std::vector<EdgeKey> intersection;
    std::set_intersection(first_set.begin(), first_set.end(), second_set.begin(), second_set.end(),
                          std::back_inserter(intersection));
    check(!intersection.empty(), "Fixture neighborhoods must overlap");
    check(!contains(first, *graph.super_arc_with_id(1)) &&
          !contains(second, *graph.super_arc_with_id(0)),
          "Fixture centers must not cover each other");

    const auto neighborhood = select(graph, 3, 2, solution);
    auto expected = first;
    expected.insert(expected.end(), second.begin(), second.end());
    check(edge_set(neighborhood) == edge_set(expected),
          "Partially overlapping neighborhoods must both be selected");
}
}

int main() {
    try {
        check_weight_cutoff_and_k_limit();
        check_candidate_boundaries();
        check_covered_center_uses_next_candidate();
        check_paired_orientation_uses_next_candidate();
        check_overlapping_neighborhoods_are_allowed();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
