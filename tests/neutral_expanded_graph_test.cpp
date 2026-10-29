#include <graph/NeutralExpandedGraph.h>
#include <graph/SuperGraph.h>
#include <io/ClusterAssignmentReader.h>
#include <io/InstanceReader.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <iostream>
#include <sstream>
#include <set>
#include <stdexcept>
#include <type_traits>

static_assert(std::has_virtual_destructor_v<Graph>);
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Action> void invalid(Action action, const std::string& fragment) {
    try { action(); }
    catch (const std::invalid_argument& error) {
        check(std::string(error.what()).find(fragment) != std::string::npos, error.what());
        return;
    }
    throw std::runtime_error("Expected invalid_argument");
}
// Construct a concrete expanded depot circuit by physical orientation IDs.
Solution path_solution(const SuperGraph& graph, const std::vector<int>& physical,
                       const std::set<int>& service_ids = {}) {
    Solution solution;
    int previous = graph.deposit();
    auto connector = [&](int from, int to, bool deposit) {
        for (const auto& arc : graph.arcs()) {
            if (arc.from != from || arc.to != to) continue;
            if (deposit) solution.deposit_traversals.emplace_back(arc.id,
                from == graph.deposit() ? -1 : from,
                to == graph.deposit() ? -1 : to, 1, 1);
            else solution.traversals.emplace_back(arc, 1);
            return;
        }
        throw std::runtime_error("Missing test connector");
    };
    for (int id : physical) {
        const auto& arc = *graph.super_arc_with_id(id);
        connector(previous, arc.from, previous == graph.deposit());
        if (service_ids.count(id)) solution.service.emplace_back(arc, 1);
        else solution.traversals.emplace_back(arc, 1);
        previous = arc.to;
    }
    connector(previous, graph.deposit(), true);
    return solution;
}

int main(int argc, char** argv) {
    try {
        const std::filesystem::path fixtures = argc > 1 ? argv[1] : "tests/fixtures";
        std::ifstream graph_file(fixtures / "neutral_binary.dat");
        std::istringstream no_turns("0 0\n");
        Instance input = InstanceReader::read(graph_file, no_turns);
        std::ifstream clusters(fixtures / "neutral_binary.clusters.dat");
        std::ifstream neutral(fixtures / "neutral_binary.neutral.dat");
        ClusterAssignmentReader::read(input, clusters, &neutral);
        check(input.record_clusters == std::vector<int>({1,2,2,2}) && input.neutral_nodes == std::vector<int>({1,2,3}), "Reader indexing");
        auto read_bad = [&](std::string rows, std::string nodes = "node_id reasons\n") {
            std::istringstream assignments("arista cluster origen destino vehiculo servicio recorridos pasadas\n" + rows);
            std::istringstream neutrals(nodes);
            Instance instance;
            instance.nodes = 2; instance.edges = {{0,1,1,1,1}};
            ClusterAssignmentReader::read(instance, assignments, &neutrals);
        };
        invalid([&]{read_bad("");}, "faltan");
        invalid([&]{read_bad("1 1 1 2 0 0 0 0\n1 1 1 2 0 0 0 0\n");}, "duplicado");
        invalid([&]{read_bad("1 1 2 1 0 0 0 0\n");}, "extremos");
        invalid([&]{read_bad("1 0 1 2 0 0 0 0\n");}, "positivo");
        invalid([&]{read_bad("1 1 1 2 0 1 0 0\n");}, "cero");
        invalid([&]{read_bad("1 1 1 2 0 0 0 0\n", "node_id reasons\n2 street\n2 street\n");}, "duplicado");
        invalid([&]{read_bad("1 1 1 2 0 0 0 0\n", "node_id reasons\n3 street\n");}, "rango");
        const Instance from_files((fixtures / "neutral_binary.dat").string(),
                                  (fixtures / "turns.dat").string(),
                                  (fixtures / "neutral_binary.clusters.dat").string(),
                                  (fixtures / "neutral_binary.neutral.dat").string());
        check(from_files.nodes == input.nodes && from_files.edges.size() == input.edges.size() &&
              from_files.arcs.size() == input.arcs.size() &&
              from_files.record_clusters == input.record_clusters && from_files.neutral_nodes == input.neutral_nodes,
              "Instance constructor did not read graph/clusters/neutral nodes");
        const Instance without_neutrals((fixtures / "neutral_binary.dat").string(),
                                        (fixtures / "turns.dat").string(),
                                        (fixtures / "neutral_binary.clusters.dat").string());
        check(without_neutrals.neutral_nodes.empty(), "Default neutral path must declare no neutral nodes");
        const NeutralExpandedGraph expanded(input);
        const auto& instance = expanded.expanded_instance();
        std::ifstream golden_file(fixtures / "neutral_binary.expanded.golden.dat");
        std::istringstream golden_turns("0 0\n");
        const Instance golden = InstanceReader::read(golden_file, golden_turns);
        auto check_records = [](const auto& actual, const auto& expected) {
            check(actual.size() == expected.size(), "Golden record count");
            for (size_t id = 0; id < actual.size(); ++id)
                check(actual[id].from == expected[id].from && actual[id].to == expected[id].to &&
                      actual[id].zone == expected[id].zone && actual[id].cost == expected[id].cost &&
                      actual[id].demand == expected[id].demand, "Golden topology/record mismatch");
        };
        check(instance.nodes == golden.nodes && instance.deposit_nodes == golden.deposit_nodes, "Golden nodes/depot mismatch");
        check_records(instance.edges, golden.edges);
        check_records(instance.arcs, golden.arcs);
        check(instance.nodes == 8 && expanded.expansions().size() == 3, "Exactly one copy per neutral");
        check(instance.edges.size() == 3 && instance.arcs.size() == 9, "Exactly two layers and two directed connectors per neutral");
        check(expanded.original_cluster_costs().at(1) == 1.25 && expanded.original_cluster_costs().at(2) == 10.75, "Original costs counted once with decimals");
        int required = 0;
        for (const auto& edge : instance.edges) required += edge.zone != 0;
        for (const auto& arc : instance.arcs) required += arc.zone != 0;
        check(required == 3, "Required service duplicated");
        check(instance.edges[2].from == 5 && instance.edges[2].to == 6 && instance.edges[2].zone == 0, "P' undirected copy");
        check(instance.arcs[1].from == 6 && instance.arcs[1].to == 7 && instance.arcs[1].zone == 0, "P' directed copy");
        const SuperGraph super_graph(expanded, expanded.expanded_turns(), expanded.expanded_illegal_turns());
        check(expanded.neutral_in_outs().size() == 2 * expanded.expansions().size(), "Two crossing entries per neutral");
        size_t crossing_id = 0;
        for (const auto& node : expanded.expansions()) {
            for (int record : {node.forward_connector_record_id, node.backward_connector_record_id}) {
                int matches = 0;
                for (const auto& arc : super_graph.arcs()) if (arc.edge_id == record) {
                    ++matches;
                    check(arc.pair == -1 && arc.cost == 12 && arc.demand == 0 && !arc.requested, "Directed in-out identity/cost");
                    const auto& crossing = expanded.neutral_in_outs().at(crossing_id++);
                    const bool forward = record == node.forward_connector_record_id;
                    check(crossing.edge == EdgeKey(arc.from, arc.to) &&
                          crossing.source_cluster == (forward ? node.original_side_cluster : node.copied_side_cluster) &&
                          crossing.target_cluster == (forward ? node.copied_side_cluster : node.original_side_cluster),
                          "Crossing metadata must match canonical SuperGraph endpoints and direction");
                    const auto& in_out = expanded.in_out_edges_by_cluster().at(crossing.source_cluster);
                    check(std::count(in_out.begin(), in_out.end(), crossing.edge) == 1, "Crossing must appear once in its source cluster");
                }
                check(matches == 1 && expanded.arc_clusters()[record] == 0, "One SuperArc per in-out record");
            }
        }
        const auto long_path = expanded.contract_solution(super_graph,
            path_solution(super_graph, {0, 2, 6, 13, 8}, {0, 2, 8}));
        check(long_path.objective == 12 && long_path.service.size() == 3, "Long binary path contraction");
        // Original deposit access is carried to both sides when physical records use both.
        Instance neutral_depot = input;
        neutral_depot.deposit_nodes = {2};
        neutral_depot.illegal_turns = {{1,2,3}};
        neutral_depot.turns = {{0,1,2}};
        const NeutralExpandedGraph sides(neutral_depot);
        check(sides.expanded_instance().deposit_nodes == std::vector<int>({2,6}), "Neutral depot sides");
        check(sides.expanded_illegal_turns().size() == 2, "Forbidden turn must stay within each side");
        for (const auto& turn : sides.expanded_illegal_turns())
            check((turn.v == 1 && turn.w == 2 && turn.u == 3) ||
                  (turn.v == 5 && turn.w == 6 && turn.u == 7), "Cross-side turn introduced");
        Instance u_turn = input;
        u_turn.deposit_nodes = {1};
        const NeutralExpandedGraph u_graph(u_turn);
        const SuperGraph u_super(u_graph, u_graph.expanded_turns(), u_graph.expanded_illegal_turns());
        const auto u_solution = path_solution(u_super, {2, 11, 5}, {2});
        invalid([&]{u_graph.contract_solution(u_super, u_solution);}, "giro inexistente o prohibido");
        Instance duplicate = input;
        duplicate.neutral_nodes.push_back(1);
        invalid([&]{NeutralExpandedGraph graph(duplicate);}, "duplicado");
        Instance overflow = input;
        overflow.arcs[0].cost = 1e308;
        overflow.arcs[1].cost = 1e308;
        invalid([&]{NeutralExpandedGraph graph(overflow);}, "Suma de costos invalida");
        Instance undeclared = input; undeclared.neutral_nodes.clear();
        invalid([&]{ NeutralExpandedGraph graph(undeclared); }, "Nodo 2 no declarado neutro: clusters 1 2");
        Instance triple = input; triple.record_clusters[3] = 3;
        invalid([&]{ NeutralExpandedGraph graph(triple); }, "exactamente dos");
        Instance single = input; single.record_clusters = {1,1,1,1};
        invalid([&]{ NeutralExpandedGraph graph(single); }, "exactamente dos");
        // Mixed canonical numbering agrees with the ordinary SuperGraph.
        Instance ordinary = input; ordinary.neutral_nodes.clear(); ordinary.record_clusters.clear();
        const Graph original_graph(ordinary);
        const SuperGraph original_super(original_graph, {}, {});
        const auto canonical = canonical_orientations(ordinary);
        for (size_t id = 0; id < canonical.size(); ++id) {
            const auto& arc = *original_super.super_arc_with_id(id);
            check(arc.from == canonical[id].virtual_from && arc.to == canonical[id].virtual_to && arc.edge_id == canonical[id].record, "Canonical numbering mismatch");
        }
        // Preserve a complete extra physical loop: X once, Y once, no pruning.
        Instance cycle;
        cycle.vehicles = 1; cycle.nodes = 6; cycle.deposit_nodes = {0};
        cycle.arcs = {{0,1,1,1.25,1},{1,2,1,2.5,1},{2,0,1,3.75,1},
                      {3,4,0,1,0},{4,5,0,1,0},{5,3,0,1,0}};
        cycle.record_clusters = {1,1,1,1,1,1};
        const NeutralExpandedGraph no_neutrals(cycle);
        check(no_neutrals.expanded_instance().nodes == 6 && no_neutrals.neutral_in_outs().empty(), "No neutral copies expected");
        const SuperGraph cycle_super(no_neutrals, {}, {});
        Solution loop = path_solution(cycle_super, {0,1,2}, {0,1,2});
        const Solution extra = path_solution(cycle_super, {0,1,2});
        // Keep one depot entry/exit and insert the second loop at node 0.
        loop.traversals.insert(loop.traversals.end(), extra.traversals.begin(), extra.traversals.end());
        for (const auto& arc : cycle_super.arcs()) if (arc.edge_id == -1 && arc.from == 5 && arc.to == 0)
            loop.traversals.emplace_back(arc, 1);
        const auto contracted = no_neutrals.contract_solution(cycle_super, loop);
        check(contracted.objective == 15 && contracted.service.size() == 3, "Contraction changed physical loop or service");
        long long physical_y = 0;
        for (const auto& value : contracted.traversals) if (value.id < 6) physical_y += value.value;
        check(physical_y == 3, "Contraction removed a physical cycle");
        for (const auto& arc : cycle_super.arcs()) {
            if ((arc.edge_id >= 3 && arc.edge_id <= 5) ||
                (arc.edge_id == -1 && arc.from >= 6 && arc.to >= 6)) loop.traversals.emplace_back(arc, 1);
        }
        invalid([&]{no_neutrals.contract_solution(cycle_super, loop);}, "desconectada");
        // A cross-side forbidden turn is never silently reintroduced at contraction.
        Instance forbidden;
        forbidden.vehicles = 1; forbidden.nodes = 3; forbidden.deposit_nodes = {0,2};
        forbidden.arcs = {{0,1,1,7,1},{1,2,1,11,1}};
        forbidden.record_clusters = {1,2}; forbidden.neutral_nodes = {1};
        forbidden.illegal_turns = {{0,1,2}};
        const NeutralExpandedGraph forbidden_graph(forbidden);
        const SuperGraph forbidden_super(forbidden_graph, forbidden_graph.expanded_turns(), forbidden_graph.expanded_illegal_turns());
        const Solution illegal = path_solution(forbidden_super, {0,2,1}, {0,1});
        invalid([&]{forbidden_graph.contract_solution(forbidden_super, illegal);}, "giro inexistente o prohibido");
        std::cout << "PASS neutral expansion, reader, canonical numbering and contraction\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
