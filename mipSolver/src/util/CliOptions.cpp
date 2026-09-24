#include "../../lib/util/CliOptions.h"
#include <stdexcept>

const char* solver_strategy_name(SolverStrategy strategy) {
    switch (strategy) {
        case SolverStrategy::Mip: return "mip";
        case SolverStrategy::FixAndOptimize: return "fixAndOptimize";
    }
    return "unknown";
}

const char* selection_strategy_name(SelectionStrategy strategy) {
    switch (strategy) {
        case SelectionStrategy::Random: return "random";
        case SelectionStrategy::MaxDeadheadCost: return "maxDeadheadCost";
        case SelectionStrategy::TopKDeadheadCost: return "topKDeadheadCost";
    }
    return "unknown";
}

CliOptions CliOptions::parse(int argc, const char* const* argv) {
    CliOptions result;

    if (argc < 3 || argc > 5)
        throw std::invalid_argument(
            "Uso: solverExec <input.dat> <curvas.dat> "
            "[mip|fixAndOptimize "
            "[maxDeadheadCost|random|topKDeadheadCost]]");

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.empty())
            throw std::invalid_argument("La ruta de entrada esta vacia");
    }

    result.graph_path = argv[1];
    result.turns_path = argv[2];
    if (argc == 3)
        return result;

    const std::string strategy = argv[3];
    if (strategy == "mip") {
        if (argc != 4)
            throw std::invalid_argument("mip no recibe una estrategia de seleccion");
        return result;
    }
    if (strategy != "fixAndOptimize")
        throw std::invalid_argument("strategy debe ser 'mip' o 'fixAndOptimize'");

    result.strategy = SolverStrategy::FixAndOptimize;
    if (argc == 4)
        return result;

    const std::string selection_strategy = argv[4];
    if (selection_strategy == "random") {
        result.selection_strategy = SelectionStrategy::Random;
    } else if (selection_strategy == "maxDeadheadCost") {
        result.selection_strategy = SelectionStrategy::MaxDeadheadCost;
    } else if (selection_strategy == "topKDeadheadCost") {
        result.selection_strategy = SelectionStrategy::TopKDeadheadCost;
    } else {
        throw std::invalid_argument(
            "selection strategy debe ser 'maxDeadheadCost', 'random' o "
            "'topKDeadheadCost'");
    }
    return result;
}
