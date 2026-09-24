#ifndef CLI_OPTIONS_H
#define CLI_OPTIONS_H

#include "../heuristic/SelectionStrategy.h"
#include <string>

enum class SolverStrategy {
    Mip,
    FixAndOptimize,
};

const char* solver_strategy_name(SolverStrategy strategy);
const char* selection_strategy_name(SelectionStrategy strategy);

struct CliOptions {
    bool help = false;
    std::string graph_path, turns_path;
    SolverStrategy strategy = SolverStrategy::Mip;
    SelectionStrategy selection_strategy = SelectionStrategy::MaxDeadheadCost;

    static CliOptions parse(int argc, const char* const* argv);
};

#endif
