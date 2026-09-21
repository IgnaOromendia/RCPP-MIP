#ifndef CLI_OPTIONS_H
#define CLI_OPTIONS_H

#include "../heuristic/SelectionStrategy.h"
#include <string>

enum class SolverStrategy {
    Mip,
    FixAndOptimize,
};

struct CliOptions {
    bool help = false;
    std::string graph_path, turns_path;
    int reachability = -1;
    SolverStrategy strategy = SolverStrategy::Mip;
    SelectionStrategy selection_strategy = SelectionStrategy::DeadheadCost;

    static CliOptions parse(int argc, const char* const* argv);
};

#endif
