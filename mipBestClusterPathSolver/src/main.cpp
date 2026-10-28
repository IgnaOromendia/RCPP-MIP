#include <io/BestClusterPathInstanceReader.h>
#include <io/SolutionWriter.h>
#include <graph/Graph.h>
#include <graph/SuperGraph.h>
#include <model/BestClusterPathSolver.h>
#include <model/ClusterBoundary.h>
#include <model/ClusterSubProblemBuilder.h>
#include <model/ClusterRCPPSolver.h>
#include <model/ClusterSolutionMerger.h>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

int main(int argc, char** argv) {
    const auto start = chrono::steady_clock::now();
    int exit_code = 0;
    SolveResult result;
    bool final_has_solution = false;
    bool all_optimal = false;
    double final_distance = 0;
    long long final_crossings = 0;

    try {
        if (argc < 5 || argc > 6) {
            cerr << "Uso: bestClusterPathExec <input.dat> <curvas.dat> <aristas.in-out.dat> <arc-clusters.dat> [salida.dat]\n";
            return 1;
        }

        const BestClusterPathInstance instance = BestClusterPathInstanceReader::read_files(argv[1], argv[2], argv[3], argv[4]);
        const Graph graph(instance.rcpp);
        const SuperGraph super_graph(graph, instance.rcpp.turns, instance.rcpp.illegal_turns);

        BestClusterPathSolver solver(super_graph, instance.in_out_edges_by_cluster);
        solver.generate_MIP();
        result = solver.solve(0.01);

        if (not result.has_solution) {
            cerr << "No se encontro solucion.\n";
            return result.status == IloAlgorithm::Error ? 1 : 2;
        }

        const Solution global_solution = result.extract_solution();

        ClusterBoundaryExtractor boundary_extractor(super_graph, instance, global_solution);
        map<int, ClusterBoundary> boundaries =boundary_extractor.get_boundaries();

        ClusterSubProblemBuilder subproblem_builder(instance, super_graph, global_solution, boundaries);
        vector<ClusterProblem> subproblems = subproblem_builder.get_subproblems();
        for (const ClusterProblem& problem : subproblems)
            for (const ClusterVisit& visit : problem.boundary.visits)
                if (visit.exit.kind == ClusterPortalKind::NeutralInOut)
                    final_crossings += visit.exit.count;

        ClusterSolutionMerger solution_merger(
            super_graph,
            instance,
            subproblems,
            boundary_extractor.get_global_arc_by_edge());

        vector<Solution> cluster_solutions;
        cluster_solutions.reserve(subproblems.size());
        all_optimal = result.status == IloAlgorithm::Optimal;

        for (ClusterProblem& problem : subproblems) {
            // En caso de que quede un cluster con no obligatorias
            if (problem.boundary.optimized_visit_count() == 0) {
                solution_merger.add_solution({});
                continue; 
            }

            const auto cluster_start = chrono::steady_clock::now();
            ClusterRCPPSolver cluster_solver(*problem.super_graph, problem.boundary);

            cluster_solver.generate_MIP();
            const SolveResult cluster_result = cluster_solver.solve(0.01);
            const auto cluster_end = chrono::steady_clock::now();
            const double cluster_elapsed_ms =
                chrono::duration<double, milli>(cluster_end - cluster_start).count();
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

        const Solution optimized = solution_merger.build_solution();
        final_distance = optimized.objective;

        const filesystem::path output = SolutionWriter::write_output(
            optimized,
            instance.rcpp.nodes,
            argc == 6 ? filesystem::path(argv[5]) : filesystem::path(),
            "min_cluster_path");
            
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
    cout << '\n';
    return exit_code;
}
