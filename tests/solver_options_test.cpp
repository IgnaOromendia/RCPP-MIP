#include "TestInstance.h"
#include "lib/RCPPSolver.h"
#include "lib/SolutionWriter.h"
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void build(RCPPSolver& solver) {
    solver.generate_MIP();
    solver.set_time_objective();
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Usage: solver_options_test <fixtures>");
        auto instance = test_instance(std::filesystem::path(argv[1]) / "feasible.dat");
        for (double capacity : {2.0, 3.0}) {
            RCPPSolver solver(test_super_graph(instance), instance.vehicles, {capacity, 10000});
            build(solver);
            check(solver.solve(0).has_solution == (capacity == 3), "Capacity threshold");
        }
        // The only route is D->0->1->2->0->D. Service costs 7, deadheading 2+3.
        // Demand/capacity are 0.25, but deadheading and turn connectors need Y=1.
        instance.nodes = 3;
        instance.deposit_nodes = {0};
        instance.arcs = {{0, 1, 1, 7, 0.25}, {1, 2, 0, 2, 0}, {2, 0, 0, 3, 0}};
        Solution snapshot;
        for (int max_traversals : {0, 1, 10000}) {
            RCPPSolver solver(test_super_graph(instance), 1, {0.25, max_traversals});
            build(solver);
            const auto result = solver.solve(0);
            if (max_traversals == 0) {
                check(!result.has_solution && result.status == IloAlgorithm::Infeasible,
                      "No deadheading must exclude the only route");
                continue;
            }
            check(result.has_solution && std::abs(solver.get_obj_value() - 12) < 1e-6,
                  "Traversal bound must be independent of load capacity");
            snapshot = solver.extract_solution();
            long long service = 0;
            for (const auto& arc : snapshot.service) service += arc.value;
            check(service == 1, "Required arc served exactly once");
            bool deadheading = false;
            for (const auto& arc : snapshot.traversals) {
                check(arc.value >= 0 && arc.value <= max_traversals, "Traversal bound");
                deadheading |= arc.value == 1;
            }
            check(deadheading, "Route must contain a traversal above fractional capacity");
            double delivered = 0;
            for (const auto& arc : snapshot.deposit_flow) delivered += arc.value;
            check(std::abs(delivered - 0.25) < 1e-6, "Deposit flow equals demand");
        }
        // Extraction owns its data independently of the environment's lifetime.
        std::ostringstream output;
        SolutionWriter::write(output, snapshot);
        check(output.str().find("OBJ: 12\n") == 0, "Snapshot survived solver destruction");
        for (const auto options : {ModelOptions{0, 1}, ModelOptions{-1, 1},
                                   ModelOptions{std::numeric_limits<double>::infinity(), 1},
                                   ModelOptions{1, -1}}) {
            bool caught = false;
            try { RCPPSolver solver(test_super_graph(instance), 1, options); }
            catch (const std::invalid_argument&) { caught = true; }
            check(caught, "API must reject invalid model options");
        }
        return 0;
    } catch (const IloException& error) {
        std::cerr << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
