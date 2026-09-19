#include "TestInstance.h"
#include "lib/RCPPSolver.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace fs = std::filesystem;

struct RCPPSolverTestAccess {
    using Environment = RCPPSolver::Environment;
};

template<typename T>
constexpr bool exclusive_owner =
    !std::is_copy_constructible_v<T> && !std::is_copy_assignable_v<T> &&
    !std::is_move_constructible_v<T> && !std::is_move_assignable_v<T>;

static_assert(exclusive_owner<RCPPSolver>);
static_assert(exclusive_owner<RCPPSolverTestAccess::Environment>);
static_assert(std::is_nothrow_destructible_v<RCPPSolver>);
static_assert(std::is_nothrow_destructible_v<RCPPSolverTestAccess::Environment>);

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void build(RCPPSolver& solver) {
    solver.generate_MIP();
    solver.set_time_objective();
}

void solve_and_check(RCPPSolver& solver) {
    const auto result = solver.solve(0);
    check(result.has_solution && result.status == IloAlgorithm::Optimal,
          "Expected an optimal solution");
    check(std::abs(solver.get_obj_value() - 7.0) < 1e-6, "Incorrect objective");
}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "Usage: solver_lifetime_test <fixtures> <case>");
        const fs::path fixtures = fs::absolute(argv[1]);
        const auto graph = (fixtures / "feasible.dat").string();
        const auto instance = test_instance(graph);
        const std::string scenario = argv[2];
        constexpr int repetitions = 20;

        if (scenario == "unbuilt") {
            for (int i = 0; i < repetitions; ++i) {
                RCPPSolver solver(test_super_graph(instance), instance.vehicles);
                check(!solver.is_feasible(), "Unexpected solution before building");
            }
        } else if (scenario == "repeated") {
            for (int i = 0; i < repetitions; ++i) {
                RCPPSolver solver(test_super_graph(instance), instance.vehicles);
                build(solver);
                solve_and_check(solver);
            }
        } else if (scenario == "independent") {
            auto first = std::make_unique<RCPPSolver>(test_super_graph(instance), instance.vehicles);
            RCPPSolver second(test_super_graph(instance), instance.vehicles);
            build(*first);
            build(second);
            solve_and_check(*first);
            first.reset();
            solve_and_check(second);
        } else if (scenario == "constructor_error") {
            // Invalid model options throw after the Concert members exist,
            // exercising environment cleanup during constructor unwinding.
            for (int i = 0; i < repetitions; ++i) {
                bool caught = false;
                try {
                    RCPPSolver solver(test_super_graph(instance), instance.vehicles, ModelOptions{0, 10000});
                } catch (const std::invalid_argument&) {
                    caught = true;
                }
                check(caught, "Expected construction to reject invalid options");
            }
            RCPPSolver recovered(test_super_graph(instance), instance.vehicles);
            build(recovered);
            solve_and_check(recovered);
        } else if (scenario == "use_error") {
            for (int i = 0; i < repetitions; ++i) {
                bool caught = false;
                try {
                    RCPPSolver solver(test_super_graph(instance), instance.vehicles);
                    build(solver);
                    solve_and_check(solver);
                    // Catch outside the scope so destruction occurs while
                    // the CPLEX exception is propagating to its handler.
                    solver.solve(-1);
                } catch (const IloException&) {
                    caught = true;
                }
                check(caught, "Expected CPLEX to reject a negative gap");
            }
            RCPPSolver recovered(test_super_graph(instance), instance.vehicles);
            build(recovered);
            solve_and_check(recovered);
        } else {
            throw std::runtime_error("Unknown lifetime scenario: " + scenario);
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
