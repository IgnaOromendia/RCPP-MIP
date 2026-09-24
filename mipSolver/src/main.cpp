#include "../lib/model/RCPPSolver.h"
#include "../lib/heuristic/FixAndOptimize.h"
#include "../lib/graph/Graph.h"
#include "../lib/io/InstanceReader.h"
#include "../lib/io/SolutionWriter.h"
#include "../lib/util/CliOptions.h"
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <stdexcept>

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
        const Instance instance = InstanceReader::read_files(options.graph_path, options.turns_path);

        const Graph graph(instance);
        const SuperGraph superGraph(graph, instance.turns, instance.illegal_turns);

        if (options.strategy == SolverStrategy::Mip) {
            RCPPSolver solver(superGraph, instance.vehicles);
            solver.generate_MIP();
            solver.set_time_objective();
            result = solver.solve(0.05);
        } else {
            FixAndOptimize solver(superGraph, instance.vehicles);
            result = solver.solve(options.selection_strategy);
        }

        if (result.has_solution) {
            cout << "Funcion objetivo: " << result.get_obj_value() << endl;
            SolutionWriter::write_file("out.dat", result.extract_solution());
        } else {
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
