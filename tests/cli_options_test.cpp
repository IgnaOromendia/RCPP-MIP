#include "lib/CliOptions.h"
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
        check(parse({"solver"}).help && parse({"solver", "--help"}).help, "Help");
        const auto defaults = parse({"solver", "graph.dat", "turns.dat"});
        check(defaults.output_path == "out.dat" && defaults.model.capacity == 10000 &&
              defaults.model.max_traversals == 10000 && defaults.gap == 0 && defaults.cuts_mode == -1,
              "Compatibility defaults");
        const auto configured = parse({"solver", "--capacity", "0.25", "graph.dat", "turns.dat",
                                       "0.01", "-1", "--output", "folder with spaces/result.dat",
                                       "--max-traversals", "2"});
        check(configured.model.capacity == 0.25 && configured.model.max_traversals == 2 &&
              configured.gap == 0.01 && configured.cuts_mode == -1 &&
              configured.output_path == "folder with spaces/result.dat", "Mixed CLI arguments");
        check(parse({"solver", "g", "t", "--max-traversals", "0"}).model.max_traversals == 0,
              "Zero traversal limit is supported");
        check(parse({"solver", "--", "--graph", "t"}).graph_path == "--graph", "Literal paths");
        for (const auto args : {
                std::initializer_list<const char*>{"solver", "g"},
                {"solver", "g", "t", "bad"}, {"solver", "g", "t", "0", "1.5"},
                {"solver", "g", "t", "nan"}, {"solver", "g", "t", "0", "-1", "extra"},
                {"solver", "g", "t", "--unknown"}, {"solver", "g", "t", "--output"},
                {"solver", "g", "t", "--output", ""},
                {"solver", "g", "t", "--capacity", "0"},
                {"solver", "g", "t", "--capacity", "-1"},
                {"solver", "g", "t", "--capacity", "inf"},
                {"solver", "g", "t", "--capacity", "3x"},
                {"solver", "g", "t", "--max-traversals", "-1"},
                {"solver", "g", "t", "--max-traversals", "0.5"},
                {"solver", "g", "t", "--max-traversals", "2147483648"}}) {
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
