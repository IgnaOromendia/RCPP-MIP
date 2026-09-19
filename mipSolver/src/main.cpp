#include "../lib/model/RCPPSolver.h"
#include "../lib/heuristic/FixAndOptimize.h"
#include "../lib/graph/Graph.h"
#include "../lib/io/InstanceReader.h"
#include "../lib/io/SolutionWriter.h"
#include "../lib/CliOptions.h"
#include <chrono>
#include <cstdlib>
#include <exception>
#include <stdexcept>

int main(int argc, char** argv){
    auto start = chrono::high_resolution_clock::now();

    int exit_code = 0;
    try {
        const auto options = CliOptions::parse(argc, argv);
        const Instance instance = InstanceReader::read_files(options.graph_path, options.turns_path);

        string strategy = "fo";

        const Graph graph(instance);
        const SuperGraph superGraph(graph, instance.turns, instance.illegal_turns);

        SolveResult result;

        if (strategy == "mip") {
            RCPPSolver solver(superGraph, instance.vehicles);
            solver.generate_MIP();
            solver.set_time_objective();
            result = solver.solve();
        } else if (strategy == "fo") {
            FixAndOptimize solver(superGraph, instance.vehicles);
            result = solver.solve();
        }

        if (result.has_solution) {
            cout << "Funcion objetivo: " << result.get_obj_value()
                 << " (" << result.status << ")" << endl;
            SolutionWriter::write_file("out.dat", result.extract_solution());
        } else {
            cerr << "No se encontro solucion. Status: " << result.status << endl;
            exit_code = result.status == IloAlgorithm::Error ? 1 : 2;
        }
    } catch (const IloException& error) {
        cerr << "Error de CPLEX: " << error << endl;
        exit_code = 1;
    } catch (const std::exception& error) {
        cerr << "Error: " << error.what() << endl;
        exit_code = 1;
    }

    auto end = chrono::high_resolution_clock::now();

    auto time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << time << " ms" << endl;

    return exit_code;

}
