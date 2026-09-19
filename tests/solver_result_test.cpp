#include "lib/RCPPSolver.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

struct RCPPSolverTestAccess {
    static void stop_after_first_solution(RCPPSolver& solver) {
        solver._solver.setParam(IloCplex::Param::Preprocessing::Presolve, false);
        solver._solver.setParam(IloCplex::Param::MIP::Limits::Solutions, 1);
        solver._solver.setParam(IloCplex::Param::Threads, 1);
    }

    static void abort_before_solve(RCPPSolver& solver) {
        IloCplex::Aborter aborter(solver._env);
        solver._solver.use(aborter);
        aborter.abort();
    }
};

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string read_output() {
    std::ifstream file("out.dat");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

template<typename Action>
void expect_no_solution(Action action) {
    try {
        action();
    } catch (const std::logic_error& error) {
        check(std::string(error.what()).find("No hay una solucion") != std::string::npos,
              "Unexpected guard diagnostic");
        return;
    }
    throw std::runtime_error("Expected a controlled no-solution exception");
}

void check_no_solution(RCPPSolver& solver) {
    check(!solver.is_feasible(), "Unexpected available solution");
    expect_no_solution([&] { solver.get_obj_value(); });
    fs::remove("out.dat");
    expect_no_solution([&] { solver.export_solution(); });
    check(!fs::exists("out.dat"), "Export created a file without a solution");
    {
        std::ofstream file("out.dat");
        file << "previous solution\n";
    }
    expect_no_solution([&] { solver.export_solution(); });
    check(read_output() == "previous solution\n", "Export overwrote an existing file");
}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "Usage: solver_result_test <fixtures> <case>");
        const fs::path fixtures = fs::absolute(argv[1]);
        const std::string scenario = argv[2];
        const bool infeasible = scenario == "infeasible";
        RCPPSolver solver((fixtures / (infeasible ? "infeasible.dat" : "feasible.dat")).string(),
                          (fixtures / "turns.dat").string());
        check_no_solution(solver);
        solver.generate_MIP();
        solver.set_time_objective();
        check_no_solution(solver);

        if (scenario == "limited") RCPPSolverTestAccess::stop_after_first_solution(solver);
        if (scenario == "aborted") RCPPSolverTestAccess::abort_before_solve(solver);
        const SolveResult result = solver.solve(0, -1);
        if (infeasible || scenario == "aborted") {
            check(!result.has_solution, "Solve reported a nonexistent solution");
            check(result.status == (infeasible ? IloAlgorithm::Infeasible : IloAlgorithm::Unknown),
                  "Unexpected status without a solution");
            check_no_solution(solver);
        } else {
            check(result.has_solution && solver.is_feasible(), "Missing feasible solution");
            check(result.status == (scenario == "limited" ? IloAlgorithm::Feasible : IloAlgorithm::Optimal),
                  "Unexpected solution status");
            check(std::abs(solver.get_obj_value() - 7.0) < 1e-6, "Incorrect objective");
            solver.export_solution();
            check(read_output().find("OBJ: 7\n") == 0, "Missing exported objective");

            // A failed new attempt must invalidate the previous solution even if
            // CPLEX still retains the old incumbent internally.
            bool caught = false;
            try {
                solver.solve(-1, -1);
            } catch (const IloException&) {
                caught = true;
            }
            check(caught, "Expected CPLEX to reject a negative gap");
            check_no_solution(solver);

            check(solver.solve(0, -1).has_solution, "Could not solve again after an error");
            // Adding another objective can fail during CPLEX's automatic
            // extraction; either way the previous solution is invalidated.
            try {
                solver.set_time_objective();
            } catch (const IloException&) {
            }
            check_no_solution(solver);
        }
        std::cout << "PASS: " << scenario << '\n';
        return 0;
    } catch (const IloException& error) {
        std::cerr << "CPLEX test failure: " << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
    }
    return 1;
}
