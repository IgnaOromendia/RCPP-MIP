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
        const auto inputs = parse({"solver", "folder with spaces/graph.dat", "turns.dat"});
        check(inputs.graph_path == "folder with spaces/graph.dat" &&
              inputs.turns_path == "turns.dat" && inputs.reachability == -1 &&
              inputs.strategy == SolverStrategy::Mip, "Input paths and default strategy");
        check(parse({"solver", "g", "t", "mip"}).strategy == SolverStrategy::Mip,
              "Explicit MIP strategy");
        const auto fix_and_optimize = parse(
            {"solver", "g", "t", "fixAndOptimize", "2"});
        check(fix_and_optimize.strategy == SolverStrategy::FixAndOptimize &&
              fix_and_optimize.reachability == 2, "Fix-and-Optimize strategy");
        const auto zero = parse({"solver", "g", "t", "fixAndOptimize", "0"});
        check(zero.reachability == 0, "Zero reachability");
        const auto literal = parse({"solver", "--capacity", "500", "mip"});
        check(literal.graph_path == "--capacity" && literal.turns_path == "500" &&
              literal.strategy == SolverStrategy::Mip,
              "Arguments are literal input paths");
        check(parse({"solver", "g", "--help", "mip"}).turns_path == "--help",
              "Help is not a special option");
        for (const auto args : {
                std::initializer_list<const char*>{"solver"},
                {"solver", "g"},
                {"solver", "g", "t", "fixAndOptimize"},
                {"solver", "g", "t", "fixAndOptimize", "-1"},
                {"solver", "g", "t", "--capacity", "500"},
                {"solver", "g", "t", "--max-traversals", "2"},
                {"solver", "g", "t", "--output", "result.dat"},
                {"solver", "g", "t", "--gap", "0.01"},
                {"solver", "g", "t", "--cutsMode", "1"},
                {"solver", "g", "t", "fo"},
                {"solver", "g", "t", "FixAndOptimize", "2"},
                {"solver", "g", "t", "mip", "2"},
                {"solver", "g", "t", "fixAndOptimize", "2", "extra"},
                {"solver", "--help"}, {"solver", "-h"},
                {"solver", "", "t"}, {"solver", "g", ""},
                {"solver", "g", "t", ""},
                {"solver", "g", "t", "fixAndOptimize", ""},
                {"solver", "g", "t", "fixAndOptimize", "1.5"},
                {"solver", "g", "t", "fixAndOptimize", "abc"},
                {"solver", "g", "t", "fixAndOptimize", "999999999999999999999"}}) {
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
