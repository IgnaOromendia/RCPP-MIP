#include "../lib/model/RCPPSolver.h"
#include "../lib/heuristic/FixAndOptimize.h"
#include "../lib/graph/Graph.h"
#include "../lib/graph/NeutralExpandedGraph.h"
#include "../lib/io/ClusterAssignmentReader.h"
#include "../lib/io/InstanceReader.h"
#include "../lib/io/SolutionWriter.h"
#include "../lib/util/CliOptions.h"
#include <chrono>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace {
void read_cluster_assignments(Instance& instance, const CliOptions& options) {
    std::ifstream clusters(options.clusters_path);
    if (!clusters)
        throw std::runtime_error("Error al abrir " + options.clusters_path);

    std::ifstream neutral_nodes;
    if (options.neutral_nodes_path) {
        neutral_nodes.open(*options.neutral_nodes_path);
        if (!neutral_nodes)
            throw std::runtime_error("Error al abrir " + *options.neutral_nodes_path);
    }
    ClusterAssignmentReader::read(
        instance, clusters, options.neutral_nodes_path ? &neutral_nodes : nullptr);
}

SolveResult solve_and_write(
        const Graph& graph,
        const vector<Turn>& turns,
        const vector<Turn>& illegal_turns,
        const Instance& instance,
        const CliOptions& options,
        const NeutralExpandedGraph* expanded_graph = nullptr) {
    const set<ExtendedTurn> no_extended_turns;
    const SuperGraph super_graph(graph, turns, illegal_turns,
        expanded_graph ? expanded_graph->get_illegal_ext_turns() : no_extended_turns);
    SolveResult result;
    if (options.strategy == SolverStrategy::Mip) {
        RCPPSolver solver(super_graph, instance.vehicles);
        solver.generate_MIP();
        result = solver.solve(0.01);
    } else {
        FixAndOptimize solver(super_graph, instance.vehicles);
        result = solver.solve(options.selection_strategy);
    }

    if (!result.has_solution) return result;

    cout << "Funcion objetivo: " << result.get_obj_value() << endl;
    std::filesystem::path output_path;
    if (expanded_graph) {
        const GraphSolution expanded_solution =
            super_graph.to_graph_solution(result.extract_solution());
        output_path = SolutionWriter::write_output(
            expanded_graph->contract_solution(expanded_solution), instance.nodes);
    } else {
        output_path = SolutionWriter::write_output(
            result.extract_solution(), instance.nodes);
    }
    cout << "Solucion guardada en " << output_path.string() << '\n';
    return result;
}
}

int main(int argc, char** argv){
    const auto start = chrono::steady_clock::now();

    int exit_code = 0;
    SolveResult result;
    try {
        const auto options = CliOptions::parse(argc, argv);
        cout << "Strategy: " << solver_strategy_name(options.strategy) << '\n';
        if (options.strategy == SolverStrategy::FixAndOptimize) {
            cout << "Selection strategy: "
                 << selection_strategy_name(options.selection_strategy) << '\n';
        }
        Instance instance = InstanceReader::read_files(
            options.graph_path, options.turns_path);

        if (options.expanded_graph) {
            read_cluster_assignments(instance, options);
            const NeutralExpandedGraph graph(instance);
            result = solve_and_write(
                graph, graph.get_turns(), graph.get_illegal_turns(),
                instance, options, &graph);
        } else {
            const Graph graph(instance);
            result = solve_and_write(
                graph, instance.turns, instance.illegal_turns, instance, options);
        }

        if (!result.has_solution) {
            cerr << "No se encontro solucion." << endl;
            exit_code = result.status == IloAlgorithm::Error ? 1 : 2;
        }
    } catch (const IloException& error) {
        cerr << "Error de CPLEX: " << error << endl;
        exit_code = 1;
    } catch (const std::exception& error) {
        cerr << "Error: " << error.what() << endl;
        exit_code = 1;
    }

    const auto end = chrono::steady_clock::now();
    const double elapsed_ms = chrono::duration<double, std::milli>(end - start).count();
    const bool optimal = result.has_solution && result.status == IloAlgorithm::Optimal;

    cout << fixed << setprecision(3)
         << "Tiempo_ms: " << elapsed_ms << '\n'
         << boolalpha << "Optimo: " << optimal << '\n'
         << "RCPP_RESULT elapsed_ms=" << elapsed_ms
         << " has_solution=" << result.has_solution
         << " optimal=" << optimal
         << " objective=";
    if (result.has_solution) cout << setprecision(17) << result.get_obj_value();
    else cout << "NA";
    cout << endl;

    return exit_code;

}
