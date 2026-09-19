#include "lib/RCPPSolver.h"
#include <chrono>
#include <cstdlib>

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

    RCPPSolver solver(argv[1], argv[2]);

    solver.generate_MIP();
    solver.set_time_objective();
    solver.solve(gapTolerance, cutsMode);
    solver.export_solution();

    auto end = chrono::high_resolution_clock::now();

    auto time = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    cout << time << " ms" << endl;

    return 0;

}
