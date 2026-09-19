#include "TestInstance.h"
#include "lib/io/SolutionWriter.h"
#include "lib/model/RCPPSolver.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

struct RCPPSolverTestAccess {
    static std::vector<std::pair<IloNum, IloNum>> bounds(const RCPPSolver& solver, const Solution& solution) {
        std::vector<std::pair<IloNum, IloNum>> result;
        const auto save = [&](IloNumVar variable) {
            result.push_back({variable.getLB(), variable.getUB()});
        };
        for (const auto& arc : solution.service)
            save(solver._X[arc.from][arc.to][arc.vehicle]);
        for (const auto& arc : solution.traversals)
            save(solver._Y[arc.from][arc.to][arc.vehicle]);
        for (const auto& arc : solution.deposit_traversals)
            save(arc.from == -1 ? solver._YDK[arc.to][arc.vehicle] : solver._YKD[arc.from][arc.vehicle]);
        return result;
    }

    static const SolveResult& current_result(const RCPPSolver& solver) {
        return solver._solve_result;
    }

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

void check_no_solution(const SolveResult& result) {
    check(!result.has_solution, "Unexpected available solution");
    expect_no_solution([&] { result.get_obj_value(); });
    expect_no_solution([&] { result.extract_solution(); });
    fs::remove("out.dat");
    expect_no_solution([&] { SolutionWriter::write_file("out.dat", result.extract_solution()); });
    check(!fs::exists("out.dat"), "Export created a file without a solution");
    {
        std::ofstream file("out.dat");
        file << "previous solution\n";
    }
    expect_no_solution([&] { SolutionWriter::write_file("out.dat", result.extract_solution()); });
    check(read_output() == "previous solution\n", "Export overwrote an existing file");
}

void check_neighborhood(RCPPSolver& solver, const SolveResult& result) {
    const Solution incumbent = result.extract_solution();
    const auto original_bounds = RCPPSolverTestAccess::bounds(solver, incumbent);
    const auto check_restored = [&] {
        check(RCPPSolverTestAccess::bounds(solver, incumbent) == original_bounds,
              "Neighborhood did not restore all variable bounds");
        check(!solver.is_feasible(), "Restoring bounds did not invalidate the current result");
    };

    const auto candidate = solver.solve_neighborhood(incumbent, {}, 0);
    check(candidate.has_solution && std::abs(candidate.get_obj_value() - 7.0) < 1e-6,
          "Fixing the incumbent changed its objective");
    check_restored();

    // A free edge must ignore the incumbent value, even if it is infeasible.
    Solution modified = incumbent;
    auto& arc = modified.traversals.at(0);
    arc.value = -1;
    const auto free_candidate = solver.solve_neighborhood(modified, {{arc.from, arc.to}}, 0);
    check(free_candidate.has_solution && std::abs(free_candidate.get_obj_value() - 7.0) < 1e-6,
          "Neighborhood fixed a free edge");
    check_restored();

    bool caught = false;
    try {
        solver.solve_neighborhood(incumbent, {}, -1);
    } catch (const IloException&) {
        caught = true;
    }
    check(caught, "Expected CPLEX to reject a negative neighborhood gap");
    check_restored();
    check(std::abs(candidate.get_obj_value() - 7.0) < 1e-6,
          "Restoring bounds changed the saved candidate");
    const auto recovered = solver.solve(0);
    check(recovered.has_solution && std::abs(recovered.get_obj_value() - 7.0) < 1e-6,
          "Could not solve the original model after a neighborhood failure");
}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "Usage: solver_result_test <fixtures> <case>");
        const fs::path fixtures = fs::absolute(argv[1]);
        const std::string scenario = argv[2];
        const bool infeasible = scenario == "infeasible";
        const auto instance = test_instance(fixtures / (infeasible ? "infeasible.dat" : "feasible.dat"));
        const auto super_graph = test_super_graph(instance);
        check_no_solution(SolveResult{});
        RCPPSolver solver(super_graph, instance.vehicles);
        check_no_solution(RCPPSolverTestAccess::current_result(solver));
        solver.generate_MIP();
        solver.set_time_objective();
        check_no_solution(RCPPSolverTestAccess::current_result(solver));

        if (scenario == "limited") RCPPSolverTestAccess::stop_after_first_solution(solver);
        if (scenario == "aborted") RCPPSolverTestAccess::abort_before_solve(solver);
        const SolveResult result = solver.solve(0);
        if (infeasible || scenario == "aborted") {
            check_no_solution(result);
            check(!result.has_solution, "Solve reported a nonexistent solution");
            check(result.status == (infeasible ? IloAlgorithm::Infeasible : IloAlgorithm::Unknown),
                  "Unexpected status without a solution");
            check_no_solution(RCPPSolverTestAccess::current_result(solver));
        } else {
            check(result.has_solution && solver.is_feasible(), "Missing feasible solution");
            check(result.status == (scenario == "limited" ? IloAlgorithm::Feasible : IloAlgorithm::Optimal),
                  "Unexpected solution status");
            check(std::abs(result.get_obj_value() - 7.0) < 1e-6, "Incorrect objective");
            if (scenario == "optimal") check_neighborhood(solver, result);
            SolutionWriter::write_file("out.dat", result.extract_solution());
            check(read_output().find("OBJ: 7\n") == 0, "Missing exported objective");

            const std::string original_output = read_output();

            // A failed new attempt must invalidate the previous solution even if
            // CPLEX still retains the old incumbent internally.
            bool caught = false;
            try {
                solver.solve(-1);
            } catch (const IloException&) {
                caught = true;
            }
            check(caught, "Expected CPLEX to reject a negative gap");
            check_no_solution(RCPPSolverTestAccess::current_result(solver));

            SolutionWriter::write_file("out.dat", result.extract_solution());
            check(read_output() == original_output, "Failed solve changed saved result");
            check(std::abs(result.get_obj_value() - 7.0) < 1e-6, "Saved objective changed");

            check(solver.solve(0).has_solution, "Could not solve again after an error");
            // Adding another objective can fail during CPLEX's automatic
            // extraction; either way the previous solution is invalidated.
            try {
                solver.set_time_objective();
            } catch (const IloException&) {
            }
            check_no_solution(RCPPSolverTestAccess::current_result(solver));
            SolutionWriter::write_file("out.dat", result.extract_solution());
            check(read_output() == original_output, "Model change altered saved result");
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
