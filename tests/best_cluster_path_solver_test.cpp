#include <graph/Graph.h>
#include <graph/SuperGraph.h>
#include <io/BestClusterPathInstanceReader.h>
#include <model/BestClusterPathSolver.h>
#include <model/ClusterBoundary.h>
#include <model/ClusterRCPPSolver.h>
#include <model/ClusterSolutionMerger.h>
#include <model/ClusterSubProblemBuilder.h>
#include <model/SolutionEulerCircuit.h>
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
        constexpr int node_count = 10;
        Solution ordered_input;
        ordered_input.deposit_traversals = {
            {10, -1, 1, 1, 1}, {16, 7, -1, 1, 1},
        };
        ordered_input.service = {
            {11, 1, 2, 1, 1}, {14, 5, 6, 1, 1},
        };
        ordered_input.traversals = {
            {12, 2, 3, 1, 1}, {13, 3, 5, 1, 1},
            {15, 6, 7, 1, 1},
        };
        const std::vector<ClusterSolutionArc> ordered =
            SolutionEulerCircuit(node_count, ordered_input).circuit();
        check(ordered.size() == 7 && ordered.front().from == -1 &&
                  ordered.front().to == 1 && ordered.back().from == 7 &&
                  ordered.back().to == -1,
              "Hierholzer did not order X, Y and deposit connectors");
        for (size_t i = 1; i < ordered.size(); ++i)
            check(ordered[i - 1].to == ordered[i].from,
                  "Hierholzer returned a discontinuous circuit");

        Solution disconnected = ordered_input;
        disconnected.traversals.push_back({17, 8, 9, 1, 1});
        disconnected.traversals.push_back({18, 9, 8, 1, 1});
        const std::vector<ClusterSolutionArc> without_internal_subtour =
            SolutionEulerCircuit(node_count, disconnected).circuit();
        check(without_internal_subtour.size() == ordered.size(),
              "Hierholzer did not discard an internal Y subtour");
        for (size_t i = 0; i < ordered.size(); ++i)
            check(without_internal_subtour[i].id == ordered[i].id &&
                      without_internal_subtour[i].from == ordered[i].from &&
                      without_internal_subtour[i].to == ordered[i].to &&
                      without_internal_subtour[i].kind == ordered[i].kind,
                  "Discarding a Y subtour changed the depot circuit");

        Solution disconnected_service = ordered_input;
        disconnected_service.service.push_back({17, 8, 9, 1, 1});
        disconnected_service.service.push_back({18, 9, 8, 1, 1});
        expect_invalid([&] {
            SolutionEulerCircuit(node_count, disconnected_service).circuit();
        }, "desconectada");

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

        // Los registros sinteticos (cluster 0) conectan subproblemas, pero no
        // deben asignarse como aristas internas de ninguno de ellos.
        const Solution global_solution = result.extract_solution();
        ClusterBoundaryExtractor boundary_extractor(
            super_graph, instance, global_solution);
        std::map<int, ClusterBoundary> boundaries =
            boundary_extractor.get_boundaries();
        ClusterSubProblemBuilder subproblem_builder(
            instance, super_graph, global_solution, boundaries);
        std::vector<ClusterProblem> subproblems =
            subproblem_builder.get_subproblems();
        check(subproblems.size() == 2,
              "Synthetic cluster records changed the number of subproblems");
        for (const ClusterProblem& problem : subproblems) {
            check(problem.cluster > 0,
                  "A subproblem was created for synthetic cluster 0");
            check(problem.instance.edges.size() + problem.instance.arcs.size() == 1,
                  "A synthetic connector leaked into a cluster subproblem");
            check(problem.boundary.visits.size() == 1 &&
                      problem.instance.vehicles == 1,
                  "The global circuit was not represented as cluster visits");
            const InstanceEdge& local_record = problem.instance.edges.empty()
                ? problem.instance.arcs.front() : problem.instance.edges.front();
            check(local_record.zone == 1,
                  "The service was not assigned to its global-circuit visit");
        }

        BestClusterPathInstance inactive_instance = instance;
        inactive_instance.arc_clusters[2] = 3;
        ClusterSubProblemBuilder inactive_subproblem_builder(
            inactive_instance, super_graph, global_solution, boundaries);
        std::vector<ClusterProblem> active_subproblems =
            inactive_subproblem_builder.get_subproblems();
        check(active_subproblems.size() == 2,
              "An inactive optional cluster created a subproblem");

        ClusterSolutionMerger solution_merger(
            super_graph,
            instance,
            subproblems,
            boundary_extractor.get_global_arc_by_edge());
        for (ClusterProblem& problem : subproblems) {
            ClusterRCPPSolver subproblem_solver(
                *problem.super_graph, problem.boundary);
            subproblem_solver.generate_MIP();
            const SolveResult subproblem_result = subproblem_solver.solve(0);
            check(subproblem_result.has_solution,
                  "A cluster subproblem is not feasible");
            solution_merger.add_solution(subproblem_result.extract_solution());
        }
        const Solution merged_solution = solution_merger.build_solution();
        check(std::abs(merged_solution.objective - 18.0) < 1e-9,
              "The merged cluster solution has the wrong distance");

        Instance local_instance;
        local_instance.vehicles = 2;
        local_instance.nodes = 4;
        local_instance.deposit_nodes = {0, 1, 2, 3};
        local_instance.arcs.push_back({0, 1, -1, 7, 3});
        local_instance.arcs.push_back({2, 3, -1, 11, 4});
        const Graph local_graph(local_instance);
        const SuperGraph local_super_graph(local_graph, {}, {});
        ClusterBoundary boundary;
        boundary.cluster = 1;
        boundary.visits = {
            {41,
             {ClusterPortalKind::GlobalDeposit, -1, 0, 1, true},
             {ClusterPortalKind::GlobalDeposit, -1, 1, 1, false}, {}, false, {}},
            {42,
             {ClusterPortalKind::NeutralInOut, -1, 2, 1, true},
             {ClusterPortalKind::NeutralInOut, -1, 3, 1, false}, {}, false, {}},
        };

        InspectableClusterRCPPSolver cluster_solver(local_super_graph, boundary);
        cluster_solver.generate_MIP();
        const int departure_arc =
            local_super_graph.super_arcs_adj_depo_node().front()->id;
        check(cluster_solver.departure_bounds(departure_arc, 1).second == 1,
              "A cluster visit accepts more than one departure");
        const SolveResult cluster_result = cluster_solver.solve(0);
        check(cluster_result.has_solution,
              "Cluster solver rejected two paired visits");
        check(std::abs(cluster_result.get_obj_value() - 18.0) < 1e-9,
              "Cluster solver did not distribute service among visits");
        std::map<int, std::pair<int, int>> endpoints;
        for (const ArcValue<long long>& traversal :
             cluster_result.extract_solution().deposit_traversals) {
            if (traversal.value <= 0) continue;
            const SuperArc& arc =
                *local_super_graph.super_arc_with_id(traversal.id);
            if (traversal.from == -1)
                endpoints[traversal.vehicle].first =
                    local_super_graph.original_node(arc.to);
            if (traversal.to == -1)
                endpoints[traversal.vehicle].second =
                    local_super_graph.original_node(arc.from);
        }
        check(endpoints[1] == std::pair<int, int>(0, 1) &&
                  endpoints[2] == std::pair<int, int>(2, 3),
              "Cluster solver crossed the A1-A2 and A3-A4 visit pairs");
        return 0;
    } catch (const IloException& error) {
        std::cerr << "CPLEX test failure: " << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
    }
    return 1;
}
