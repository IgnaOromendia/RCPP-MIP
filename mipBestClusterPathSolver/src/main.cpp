#include <io/BestClusterPathInstanceReader.h>
#include <io/SolutionWriter.h>
#include <graph/Graph.h>
#include <graph/SuperGraph.h>
#include <model/BestClusterPathSolver.h>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
void print_usage() {
    std::cerr << "Uso: bestClusterPathExec <input.dat> <curvas.dat> "
                 "<aristas.in-out.dat> [salida.dat]\n";
}
}

int main(int argc, char** argv) {
    const auto start = std::chrono::steady_clock::now();
    int exit_code = 0;
    SolveResult result;

    try {
        if (argc < 4 || argc > 5) {
            print_usage();
            return 1;
        }
        const BestClusterPathInstance instance =
            BestClusterPathInstanceReader::read_files(argv[1], argv[2], argv[3]);
        const Graph graph(instance.rcpp);
        const SuperGraph super_graph(
            graph, instance.rcpp.turns, instance.rcpp.illegal_turns);
        BestClusterPathSolver solver(
            super_graph, instance.rcpp.vehicles,
            instance.in_out_edges_by_cluster);
        solver.generate_MIP();
        result = solver.solve(0.01);

        if (result.has_solution) {
            const std::filesystem::path output = argc == 5
                ? std::filesystem::path(argv[4])
                : std::filesystem::path("data") /
                    std::to_string(instance.rcpp.nodes) /
                    ("min_cluster_path_" + std::to_string(instance.rcpp.nodes) + ".dat");
            if (output.has_parent_path())
                std::filesystem::create_directories(output.parent_path());
            SolutionWriter::write_file(output.string(), result.extract_solution());
            std::cout << "Funcion objetivo: " << result.get_obj_value() << '\n'
                      << "Solucion guardada en " << output.string() << '\n';
        } else {
            std::cerr << "No se encontro solucion.\n";
            exit_code = result.status == IloAlgorithm::Error ? 1 : 2;
        }
    } catch (const IloException& error) {
        std::cerr << "Error de CPLEX: " << error << '\n';
        exit_code = 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        exit_code = 1;
    }

    const auto end = std::chrono::steady_clock::now();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
    const bool optimal = result.has_solution && result.status == IloAlgorithm::Optimal;
    std::cout << std::fixed << std::setprecision(3)
              << "Tiempo_ms: " << elapsed_ms << '\n'
              << std::boolalpha << "Optimo: " << optimal << '\n'
              << "BEST_CLUSTER_PATH_RESULT elapsed_ms=" << elapsed_ms
              << " has_solution=" << result.has_solution
              << " optimal=" << optimal << " objective=";
    if (result.has_solution)
        std::cout << std::setprecision(17) << result.get_obj_value();
    else
        std::cout << "NA";
    std::cout << '\n';
    return exit_code;
}
