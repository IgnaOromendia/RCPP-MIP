#include "../lib/RCPPSolver.h"
#include "../lib/Graph.h"
#include "../lib/InstanceReader.h"
#include "../lib/SolutionWriter.h"
#include "../lib/CliOptions.h"
#include <chrono>
#include <cstdlib>
#include <exception>

int main(int argc, char** argv){
    auto start = chrono::high_resolution_clock::now();

    int exit_code = 0;
    try {
        const auto options = CliOptions::parse(argc, argv);
        const Instance instance = InstanceReader::read_files(options.graph_path, options.turns_path);

        const Graph graph(instance);
        RCPPSolver solver(SuperGraph(graph, instance.turns, instance.illegal_turns), instance.vehicles);

        solver.generate_MIP();
        solver.set_time_objective();

        constexpr double gap = 0;
        constexpr int cutsMode = -1;
        const SolveResult result = solver.solve(gap, cutsMode);

        if (result.has_solution) {
            cout << "Funcion objetivo: " << solver.get_obj_value()
                 << " (" << result.status << ")" << endl;
            SolutionWriter::write_file("out.dat", solver.extract_solution());
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
