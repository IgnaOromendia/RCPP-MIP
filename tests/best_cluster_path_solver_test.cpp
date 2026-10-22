#include <graph/Graph.h>
#include <graph/SuperGraph.h>
#include <io/BestClusterPathInstanceReader.h>
#include <model/BestClusterPathSolver.h>
#include <model/ClusterRCPPSolver.h>
#include <model/ClusterSubProblemBuilder.h>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

static_assert(std::is_base_of_v<RCPPSolver, BestClusterPathSolver>);
static_assert(std::is_base_of_v<RCPPSolver, ClusterRCPPSolver>);

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Action>
void expect_invalid(Action action, const std::string& fragment) {
    try {
        action();
    } catch (const std::invalid_argument& error) {
        check(std::string(error.what()).find(fragment) != std::string::npos,
              "Unexpected validation diagnostic");
        return;
    }
    throw std::runtime_error("Expected invalid_argument");
}

BestClusterPathInstance read_instance(
    const std::string& in_out_contents,
    int vehicles = 1) {
    std::istringstream graph(
        std::to_string(vehicles) + " 4 2 2 2\n"
        "1 3\n"
        "1 2 1 7 3\n"
        "4 3 1 11 4\n"
        "2 4 0 18 0\n"
        "4 2 0 18 0\n");
    std::istringstream turns("0 0\n");
    std::istringstream in_out(in_out_contents);
    std::istringstream arc_clusters(
        "arc cluster\n"
        "1 1\n"
        "2 2\n"
        "3 0\n"
        "4 0\n");
    return BestClusterPathInstanceReader::read(
        graph, turns, in_out, arc_clusters);
}

class InspectableBestClusterPathSolver : public BestClusterPathSolver {
public:
    using BestClusterPathSolver::BestClusterPathSolver;

    double objective_coefficient(IloNumVar variable) const {
        for (IloExpr::LinearIterator term = _solver.getObjective().getLinearIterator();
             term.ok(); ++term) {
            if (term.getVar().getImpl() == variable.getImpl())
                return term.getCoef();
        }
        return 0;
    }

    double y_objective_coefficient(int arc_id, int vehicle) const {
        return objective_coefficient(_Y[arc_id][vehicle]);
    }

    double x_objective_coefficient(int arc_id, int vehicle) const {
        return objective_coefficient(_X[arc_id][vehicle]);
    }
};

class InspectableClusterRCPPSolver : public ClusterRCPPSolver {
public:
    using ClusterRCPPSolver::ClusterRCPPSolver;

    std::pair<IloNum, IloNum> departure_bounds(int arc, int vehicle) const {
        return get_variable_bounds(_YDK[arc][vehicle]);
    }

};

int main() {
    try {
        BestClusterPathInstance instance = read_instance(
            "source_cluster target_cluster in_node out_node\n"
            "1 2 9 10\n"
            "2 1 11 12\n");
        check(instance.in_out_edges_by_cluster.size() == 2,
              "Reader did not separate in/out edges by cluster");
        check(instance.in_out_edges_by_cluster.at(1) ==
                  std::vector<EdgeKey>({{8, 9}}),
              "Reader did not convert one-based virtual nodes");

        expect_invalid([&] {
            read_instance(
                "source_cluster target_cluster in_node out_node\n"
                "1 2 9 10\n"
                "2 1 9 10\n");
        }, "duplicada");
        expect_invalid([&] {
            read_instance(
                "source_cluster target_cluster in_node out_node\n"
                "1 2 1 99\n");
        }, "fuera de rango");
        expect_invalid([&] {
            read_instance(
                "source_cluster target_cluster in_node out_node\n"
                "1 2 9 10\n"
                "2 1 11 12\n",
                2);
        }, "exactamente un vehiculo");

        const Graph graph(instance.rcpp);
        const SuperGraph super_graph(
            graph, instance.rcpp.turns, instance.rcpp.illegal_turns);
        InspectableBestClusterPathSolver solver(
            super_graph, instance.in_out_edges_by_cluster);
        solver.generate_MIP();

        const auto& ids = solver.in_out_arc_ids_by_cluster();
        check(ids.size() == 2 && ids.at(1).size() == 1 && ids.at(2).size() == 1,
              "Solver did not preserve cluster groups");
        for (const auto& [cluster, arc_ids] : ids) {
            (void) cluster;
            check(std::abs(solver.y_objective_coefficient(arc_ids.front(), 1) - 1.0) < 1e-12,
                  "Selected Y variable does not have unit objective cost");
        }
        for (int physical_arc_id = 0; physical_arc_id < 4; ++physical_arc_id) {
            check(std::abs(solver.y_objective_coefficient(physical_arc_id, 1)) < 1e-12,
                  "Physical-edge Y variable leaked into the transition objective");
            check(std::abs(solver.x_objective_coefficient(physical_arc_id, 1)) < 1e-12,
                  "X service variable leaked into the transition objective");
        }

        const SolveResult result = solver.solve(0);
        check(result.has_solution, "Best-cluster-path model is not feasible");
        check(std::abs(result.get_obj_value() - 1.0) < 1e-9,
              "A three-node, two-cluster path must change cluster exactly once");
        long long transition_uses = 0;
        for (const ArcValue<long long>& traversal : result.extract_solution().traversals) {
            if (traversal.id == ids.at(1).front() ||
                traversal.id == ids.at(2).front())
                transition_uses += traversal.value;
        }
        check(transition_uses == 1,
              "The solution did not expose the selected cluster-change Y");

        Instance local_instance;
        local_instance.vehicles = 1;
        local_instance.nodes = 2;
        local_instance.deposit_nodes = {0, 1};
        local_instance.arcs.push_back({0, 1, 1, 7, 3});
        const Graph local_graph(local_instance);
        const SuperGraph local_super_graph(local_graph, {}, {});
        ClusterBoundary boundary;
        boundary.cluster = 1;
        boundary.gates = {
            {0, 2, 0},
            {1, 0, 2},
        };

        InspectableClusterRCPPSolver cluster_solver(local_super_graph, boundary);
        cluster_solver.generate_MIP();
        const int departure_arc =
            local_super_graph.super_arcs_adj_depo_node().front()->id;
        check(cluster_solver.departure_bounds(departure_arc, 1).second == 2,
              "Cluster entry variable did not accept multiplicity two");
        const SolveResult cluster_result = cluster_solver.solve(0);
        check(cluster_result.has_solution,
              "Cluster solver rejected two entries and two exits");
        check(std::abs(cluster_result.get_obj_value() - 14.0) < 1e-9,
              "Cluster solver did not honor boundary multiplicity");
        long long entries = 0;
        long long exits = 0;
        for (const ArcValue<long long>& traversal :
             cluster_result.extract_solution().deposit_traversals) {
            if (traversal.from == -1) entries += traversal.value;
            if (traversal.to == -1) exits += traversal.value;
        }
        check(entries == 2 && exits == 2,
              "Cluster solver changed its fixed boundary");
        return 0;
    } catch (const IloException& error) {
        std::cerr << "CPLEX test failure: " << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
    }
    return 1;
}
