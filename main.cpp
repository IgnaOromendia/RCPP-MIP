#include "lib/RCPPSolver.h"
#include <chrono>
#include <cstdlib>
#include <exception>

int main(int argc, char** argv){
    if(argc < 3){
        cout << "./solverExec <input.dat> <curvas.dat> [gap]" << endl;
        exit(0);
    }

    auto start = chrono::high_resolution_clock::now();

    double gapTolerance = 0;
    int cutsMode = -1;

    if (argc >= 4) gapTolerance = atof(argv[3]);
    if (argc >= 5) cutsMode = atoi(argv[4]);

    int exit_code = 0;
    try {
        RCPPSolver solver(argv[1], argv[2]);
        solver.generate_MIP();
        solver.set_time_objective();
        const SolveResult result = solver.solve(gapTolerance, cutsMode);
        if (result.has_solution) {
            cout << "Funcion objetivo: " << solver.get_obj_value()
                 << " (" << result.status << ")" << endl;
            solver.export_solution();
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
