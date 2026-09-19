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
              inputs.turns_path == "turns.dat", "Input paths");
        const auto literal = parse({"solver", "--capacity", "500"});
        check(literal.graph_path == "--capacity" && literal.turns_path == "500",
              "Arguments are literal input paths");
        check(parse({"solver", "g", "--help"}).turns_path == "--help",
              "Help is not a special option");
        for (const auto args : {
                std::initializer_list<const char*>{"solver"},
                {"solver", "g"},
                {"solver", "g", "t", "0.01"},
                {"solver", "g", "t", "0", "-1"},
                {"solver", "g", "t", "--capacity", "500"},
                {"solver", "g", "t", "--max-traversals", "2"},
                {"solver", "g", "t", "--output", "result.dat"},
                {"solver", "g", "t", "--gap", "0.01"},
                {"solver", "g", "t", "--cutsMode", "1"},
                {"solver", "--help"}, {"solver", "-h"},
                {"solver", "", "t"}, {"solver", "g", ""}}) {
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
