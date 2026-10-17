#include <graph/Graph.h>
#include <graph/SuperGraph.h>
#include <io/BestClusterPathInstanceReader.h>
#include <model/BestClusterPathSolver.h>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

static_assert(std::is_base_of_v<RCPPSolver, BestClusterPathSolver>);

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

BestClusterPathInstance read_instance(const std::string& in_out_contents) {
    std::istringstream graph(
        "1 3 2 0 2\n"
        "1 3\n"
        "1 2 1 7 3\n"
        "2 3 1 11 4\n");
    std::istringstream turns("0 0\n");
    std::istringstream in_out(in_out_contents);
    return BestClusterPathInstanceReader::read(graph, turns, in_out);
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

int main() {
    try {
        BestClusterPathInstance instance = read_instance(
            "cluster in_node out_node\n"
            "1 1 2\n"
            "2 3 4\n");
        check(instance.in_out_edges_by_cluster.size() == 2,
              "Reader did not separate in/out edges by cluster");
        check(instance.in_out_edges_by_cluster.at(1) ==
                  std::vector<EdgeKey>({{0, 1}}),
              "Reader did not convert one-based virtual nodes");

        expect_invalid([&] {
            read_instance(
                "cluster in_node out_node\n"
                "1 1 2\n"
                "2 1 2\n");
        }, "duplicada");
        expect_invalid([&] {
            read_instance("cluster in_node out_node\n1 1 9\n");
        }, "fuera de rango");

        const Graph graph(instance.rcpp);
        const SuperGraph super_graph(
            graph, instance.rcpp.turns, instance.rcpp.illegal_turns);
        InspectableBestClusterPathSolver solver(
            super_graph, instance.rcpp.vehicles,
            instance.in_out_edges_by_cluster);
        solver.generate_MIP();

        const auto& ids = solver.in_out_arc_ids_by_cluster();
        check(ids.size() == 2 && ids.at(1).size() == 1 && ids.at(2).size() == 1,
              "Solver did not preserve cluster groups");
        for (const auto& [cluster, arc_ids] : ids) {
            (void) cluster;
            check(std::abs(solver.y_objective_coefficient(arc_ids.front(), 1) - 1.0) < 1e-12,
                  "Selected Y variable does not have unit objective cost");
            check(std::abs(solver.x_objective_coefficient(arc_ids.front(), 1)) < 1e-12,
                  "X service variable leaked into the objective");
        }

        const SolveResult result = solver.solve(0);
        check(result.has_solution, "Best-cluster-path model is not feasible");
        check(std::abs(result.get_obj_value()) < 1e-9,
              "Unexpected in/out traversal objective");
        return 0;
    } catch (const IloException& error) {
        std::cerr << "CPLEX test failure: " << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
    }
    return 1;
}
