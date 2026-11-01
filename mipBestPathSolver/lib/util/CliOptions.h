#ifndef CLI_OPTIONS_H
#define CLI_OPTIONS_H

#include "../heuristic/SelectionStrategy.h"
#include <optional>
#include <string>

enum class SolverStrategy {
    Mip,
    FixAndOptimize,
};

const char* solver_strategy_name(SolverStrategy strategy);
const char* selection_strategy_name(SelectionStrategy strategy);

struct CliOptions {
    bool help = false;
    bool expanded_graph = false;
    std::string graph_path, turns_path;
    std::string clusters_path;
    std::optional<std::string> neutral_nodes_path;
    SolverStrategy strategy = SolverStrategy::FixAndOptimize;
    SelectionStrategy selection_strategy = SelectionStrategy::TopKDeadheadCost;

    static CliOptions parse(int argc, const char* const* argv);
};

#endif
