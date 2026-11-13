#include <io/SolutionWriter.h>
#include <graph/Graph.h>
#include <graph/SuperGraph.h>
#include <model/Instance.h>
#include <graph/NeutralExpandedGraph.h>
#include <model/BestClusterPathSolver.h>
#include <model/SubtourBreaker.h>
#include <model/ClusterBoundary.h>
#include <model/ClusterSubProblemBuilder.h>
#include <model/ClusterRCPPSolver.h>
#include <model/ClusterSolutionMerger.h>
#include <model/SolutionEulerCircuit.h>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
string audited_route(const NeutralExpandedGraph& graph, const SuperGraph& super_graph,
                     const Solution& solution, long long expected_crossings) {
    const auto circuit = SolutionEulerCircuit(super_graph.nodes_amount(), solution).circuit();
    ostringstream csv;
    csv << "vehiculo,orden,nodo_origen,nodo_destino,cluster,cluster_efectivo,arista,servicio\n";
    long long crossings = 0;
    int previous_cluster = 0;
    int pending_crossings = 0;
    int order = 0;
    for (const auto& step : circuit) {
        ++order;
        const auto& arc = *super_graph.super_arc_with_id(step.id);
        if (step.kind == SolutionArcKind::Deposit) {
            previous_cluster = 0;
            continue;
        }
        if (arc.is_neutral) {
            ++pending_crossings;
            ++crossings;
            continue;
        }
        if (arc.edge_id < 0) continue;

        const int cluster = graph.edge_cluster(arc.edge_id);
        if (graph.has_shared_streets() &&
            pending_crossings != (previous_cluster != 0 && previous_cluster != cluster))
            throw invalid_argument("El circuito no conserva las transiciones entre barrios");
        pending_crossings = 0;
        previous_cluster = cluster;
        const int from = graph.original_node_id(super_graph.original_node(arc.from));
        const int to = graph.original_node_id(super_graph.original_node(arc.to));
        csv << "1," << order << ',' << from + 1 << ',' << to + 1 << ','
            << graph.physical_cluster(arc.edge_id) << ',' << cluster << ','
            << graph.original_edge_id(arc.edge_id) + 1 << ','
            << (step.kind == SolutionArcKind::Service) << '\n';
    }
    if (crossings != expected_crossings || pending_crossings != 0)
        throw invalid_argument("El circuito fusionado no conserva el conteo de cruces");
    return csv.str();
}
}

int main(int argc, char** argv) {
    const auto start = chrono::steady_clock::now();
    int exit_code = 0;
    SolveResult result;
    bool final_has_solution = false;
    bool all_optimal = false;
    double final_distance = 0;
    long long final_crossings = 0;

    try {
        if (argc < 5 || (argc - 3) % 2 != 0) {
            cerr << "Uso: bestClusterPathExec <input.dat> <curvas.dat> --clusters <graph-clusters.dat> [--neutral-nodes <neutral-nodes.dat>] [--output <salida.dat>] [--sorter-output <salida-supergrafo.dat>] [--route-output <segmentos.csv>]\n";
            return 1;
        }

        string clusters_path, neutral_path, output_path, sorter_output_path, route_output_path;
        for (int arg = 3; arg < argc; arg += 2) {
            string option = argv[arg];
            string* destination = nullptr;
            if (option == "--clusters") destination = &clusters_path;
            else if (option == "--neutral-nodes") destination = &neutral_path;
            else if (option == "--output") destination = &output_path;
            else if (option == "--sorter-output") destination = &sorter_output_path;
            else if (option == "--route-output") destination = &route_output_path;
            else throw invalid_argument("Opcion desconocida: " + option);
            if (!destination->empty()) throw invalid_argument("Opcion duplicada: " + option);
            *destination = argv[arg + 1];
            if (destination->empty()) throw invalid_argument("Falta valor para " + option);
        }
        if (clusters_path.empty()) throw invalid_argument("Falta --clusters");

        const Instance instance(argv[1], argv[2], clusters_path, neutral_path);
        const NeutralExpandedGraph graph(instance);
        const SuperGraph super_graph(graph, graph.get_turns(), graph.get_illegal_turns(), graph.get_illegal_ext_turns());

        BestClusterPathSolver solver(super_graph, graph.get_neutral_edges());
        solver.generate_MIP();
        result = solver.solve(0.01);

        if (not result.has_solution) {
            cerr << "No se encontro solucion.\n";
            return result.status == IloAlgorithm::Error ? 1 : 2;
        }

        const SubtourBreaker subtour_breaker(super_graph, graph, result.extract_solution(), solver.get_neutral_arc_ids());
        const Solution& global_solution = subtour_breaker.get_solution();
        final_crossings = static_cast<long long>(global_solution.objective);
        cout << "Cruces antes: " << result.get_obj_value()
             << " despues: " << final_crossings << '\n';

        const map<int, ClusterBoundary> boundaries = ClusterBoundaryExtractor(
            super_graph, graph, global_solution, subtour_breaker.get_connections()).get_boundaries();

        ClusterSubProblemBuilder subproblem_builder(graph, super_graph, boundaries);
        vector<ClusterProblem> subproblems = subproblem_builder.get_subproblems();
        ClusterSolutionMerger solution_merger(
            super_graph,
            graph,
            subproblems);

        all_optimal = result.status == IloAlgorithm::Optimal &&
            !subtour_breaker.was_repaired() && !subproblem_builder.services_reassigned();

        for (ClusterProblem& problem : subproblems) {
            // En caso de que quede un cluster con no obligatorias
            if (problem.boundary.optimized_visit_count() == 0) {
                solution_merger.add_solution({});
                continue; 
            }

            const auto cluster_start = chrono::steady_clock::now();
            ClusterRCPPSolver cluster_solver(*problem.super_graph, problem.boundary,
                problem.mapping.local_to_global_node);

            cluster_solver.generate_MIP();
            const SolveResult cluster_result = cluster_solver.solve(0.01);
            const auto cluster_end = chrono::steady_clock::now();
            const double cluster_elapsed_ms = chrono::duration<double, milli>(cluster_end - cluster_start).count();
            ostringstream cluster_elapsed;
            cluster_elapsed << fixed << setprecision(3) << cluster_elapsed_ms;

            cout << "Cluster " << problem.cluster
                        << ": solucion=" << boolalpha
                        << cluster_result.has_solution
                        << " optimo="
                        << (cluster_result.has_solution &&
                            cluster_result.status == IloAlgorithm::Optimal)
                        << " tiempo_ms=" << cluster_elapsed.str()
                        << '\n';

            if (!cluster_result.has_solution) {
                cerr << "No se encontro solucion para el cluster" << problem.cluster << ".\n";
                return 2;
            }

            all_optimal = all_optimal && cluster_result.status == IloAlgorithm::Optimal;

            solution_merger.add_solution(cluster_result.extract_solution());
        }

        const Solution merged_solution = solution_merger.build_solution();
        const string route = audited_route(graph, super_graph, merged_solution, final_crossings);
        const GraphSolution expanded_graph_solution = super_graph.to_graph_solution(merged_solution);
        const GraphSolution contracted_solution = graph.contract_solution(expanded_graph_solution);

        final_distance = contracted_solution.objective;

        const filesystem::path output = SolutionWriter::write_output(
            contracted_solution,
            instance.nodes,
            filesystem::path(output_path),
            "min_cluster_path");

        if (!sorter_output_path.empty()) {
            if (filesystem::absolute(sorter_output_path).lexically_normal() ==
                filesystem::absolute(output).lexically_normal())
                throw invalid_argument("--sorter-output debe ser distinto de --output");
            const Graph original_graph(instance);
            const SuperGraph original_super(
                original_graph, instance.turns, instance.illegal_turns);
            const Solution sorter_solution = graph.contract_solution(
                merged_solution, super_graph, original_super);
            SolutionWriter::write_output(sorter_solution, instance.nodes, sorter_output_path);
            cout << "Solucion para ordenar guardada en " << sorter_output_path << '\n';
        }

        if (!route_output_path.empty()) {
            ofstream route_file(route_output_path);
            route_file << route;
            if (!route_file) throw runtime_error("Error al escribir " + route_output_path);
        }

        final_has_solution = true;
        cout << "Cambios entre clusters: "
                    << final_crossings << '\n'
                    << "Distancia optimizada: " << final_distance << '\n'
                    << "Solucion guardada en " << output.string() << '\n';

    } catch (const IloException& error) {
        cerr << "Error de CPLEX: " << error << '\n';
        exit_code = 1;
    } catch (const exception& error) {
        cerr << "Error: " << error.what() << '\n';
        exit_code = 1;
    }

    const auto end = chrono::steady_clock::now();
    const double elapsed_ms =
        chrono::duration<double, milli>(end - start).count();
    cout << fixed << setprecision(3)
              << "Tiempo_ms: " << elapsed_ms << '\n'
              << boolalpha << "Optimo: "
              << (final_has_solution && all_optimal) << '\n'
              << "BEST_CLUSTER_PATH_RESULT elapsed_ms=" << elapsed_ms
              << " has_solution=" << final_has_solution
              << " optimal=" << (final_has_solution && all_optimal)
              << " objective=";
    if (result.has_solution)
        cout << setprecision(17) << final_crossings;
    else
        cout << "NA";
    cout << " crossings=";
    if (result.has_solution)
        cout << setprecision(17) << final_crossings;
    else
        cout << "NA";
    cout << " distance=";
    if (final_has_solution)
        cout << setprecision(17) << final_distance;
    else
        cout << "NA";
    cout << " global_gap=";
    if (result.has_solution)
        cout << setprecision(17) << result.relative_gap;
    else
        cout << "NA";
    cout << '\n';
    return exit_code;
}
