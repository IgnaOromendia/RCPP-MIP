#include "lib/util/CliOptions.h"
#include <iostream>
#include <stdexcept>
#include <vector>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

CliOptions parse(std::initializer_list<const char*> args) {
    std::vector<const char*> argv(args);
    return CliOptions::parse(static_cast<int>(argv.size()), argv.data());
}

int main() {
    try {
        const auto inputs = parse({
            "solver", "folder with spaces/graph.dat", "turns.dat", "clusters.dat"});
        check(inputs.graph_path == "folder with spaces/graph.dat" &&
              inputs.turns_path == "turns.dat" &&
              inputs.clusters_path == "clusters.dat" &&
              inputs.strategy == SolverStrategy::FixAndOptimize &&
              inputs.selection_strategy == SelectionStrategy::TopKDeadheadCost,
              "Input paths and default strategies");
        check(parse({"solver", "g", "t", "c", "mip"}).strategy == SolverStrategy::Mip,
              "Explicit MIP strategy");
        check(std::string(solver_strategy_name(SolverStrategy::Mip)) == "mip" &&
              std::string(solver_strategy_name(SolverStrategy::FixAndOptimize)) ==
                  "fixAndOptimize",
              "Solver strategy names");
        check(std::string(selection_strategy_name(SelectionStrategy::MaxDeadheadCost)) ==
                  "maxDeadheadCost" &&
              std::string(selection_strategy_name(SelectionStrategy::Random)) == "random" &&
              std::string(selection_strategy_name(SelectionStrategy::TopKDeadheadCost)) ==
                  "topKDeadheadCost",
              "Selection strategy names");
        const auto fix_and_optimize = parse(
            {"solver", "g", "t", "c", "fixAndOptimize"});
        check(fix_and_optimize.strategy == SolverStrategy::FixAndOptimize &&
              fix_and_optimize.selection_strategy == SelectionStrategy::TopKDeadheadCost,
              "Fix-and-Optimize default selection strategy");
        check(parse({"solver", "g", "t", "c", "fixAndOptimize", "maxDeadheadCost"})
                  .selection_strategy == SelectionStrategy::MaxDeadheadCost,
              "Explicit deadhead-cost selection strategy");
        check(parse({"solver", "g", "t", "c", "fixAndOptimize", "random"})
                  .selection_strategy == SelectionStrategy::Random,
              "Random selection strategy");
        const auto top_k = parse(
            {"solver", "g", "t", "c", "fixAndOptimize", "topKDeadheadCost"});
        check(top_k.selection_strategy == SelectionStrategy::TopKDeadheadCost,
              "Top-k deadhead-cost selection strategy");
        const auto literal = parse({"solver", "--capacity", "500", "clusters", "mip"});
        check(literal.graph_path == "--capacity" && literal.turns_path == "500" &&
              literal.clusters_path == "clusters" &&
              literal.strategy == SolverStrategy::Mip,
              "Arguments are literal input paths");
        check(parse({"solver", "g", "--help", "c", "mip"}).turns_path == "--help",
              "Help is not a special option");
        for (const auto args : {
                std::initializer_list<const char*>{"solver"},
                {"solver", "g"},
                {"solver", "g", "t"},
                {"solver", "g", "t", "c", "--capacity", "500"},
                {"solver", "g", "t", "c", "--max-traversals"},
                {"solver", "g", "t", "c", "fo"},
                {"solver", "g", "t", "c", "FixAndOptimize"},
                {"solver", "g", "t", "c", "mip", "2"},
                {"solver", "g", "t", "c", "fixAndOptimize", "2"},
                {"solver", "g", "t", "c", "fixAndOptimize", "extra"},
                {"solver", "g", "t", "c", "fixAndOptimize", "Random"},
                {"solver", "g", "t", "c", "fixAndOptimize", "random", "extra"},
                {"solver", "--help"}, {"solver", "-h"},
                {"solver", "", "t", "c"}, {"solver", "g", "", "c"},
                {"solver", "g", "t", ""},
                {"solver", "g", "t", "c", ""},
                {"solver", "g", "t", "c", "fixAndOptimize", ""}}) {
            bool caught = false;
            try { parse(args); }
            catch (const std::invalid_argument&) { caught = true; }
            check(caught, "Accepted invalid CLI arguments");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
