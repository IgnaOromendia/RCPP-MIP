#include "../../lib/util/CliOptions.h"
#include <stdexcept>
#include <vector>

namespace {
constexpr const char* USAGE =
    "Uso: solverExec <input.dat> <curvas.dat> "
    "[mip|fixAndOptimize "
    "[maxDeadheadCost|random|topKDeadheadCost]] "
    "[--expanded-graph --clusters <graph-clusters.dat> "
    "[--neutral-nodes <neutral-nodes.dat>]]";
}

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

    if (argc < 3) throw std::invalid_argument(USAGE);

    for (int i = 1; i <= 2; ++i) {
        const std::string arg = argv[i];
        if (arg.empty())
            throw std::invalid_argument("La ruta de entrada esta vacia");
    }

    result.graph_path = argv[1];
    result.turns_path = argv[2];

    std::vector<std::string> positional;
    for (int i = 3; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument.empty()) throw std::invalid_argument("Argumento vacio");
        if (argument == "--expanded-graph") {
            if (result.expanded_graph)
                throw std::invalid_argument("Opcion duplicada: --expanded-graph");
            result.expanded_graph = true;
            continue;
        }
        if (argument == "--clusters" || argument == "--neutral-nodes") {
            if (i + 1 >= argc || std::string(argv[i + 1]).empty())
                throw std::invalid_argument("Falta valor para " + argument);
            const std::string value = argv[++i];
            if (argument == "--clusters") {
                if (!result.clusters_path.empty())
                    throw std::invalid_argument("Opcion duplicada: --clusters");
                result.clusters_path = value;
            } else {
                if (result.neutral_nodes_path)
                    throw std::invalid_argument("Opcion duplicada: --neutral-nodes");
                result.neutral_nodes_path = value;
            }
            continue;
        }
        if (argument.rfind("--", 0) == 0)
            throw std::invalid_argument("Opcion desconocida: " + argument);
        positional.push_back(argument);
    }

    if (positional.size() > 2) throw std::invalid_argument(USAGE);
    if (!positional.empty()) {
        if (positional[0] == "mip") {
            if (positional.size() != 1)
                throw std::invalid_argument("mip no recibe una estrategia de seleccion");
            result.strategy = SolverStrategy::Mip;
        } else if (positional[0] == "fixAndOptimize") {
            result.strategy = SolverStrategy::FixAndOptimize;
        } else {
            throw std::invalid_argument("strategy debe ser 'mip' o 'fixAndOptimize'");
        }
    }
    if (positional.size() == 2) {
        const std::string& selection_strategy = positional[1];
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
    }

    if (result.expanded_graph && result.clusters_path.empty())
        throw std::invalid_argument("Falta --clusters para --expanded-graph");
    if (!result.expanded_graph &&
            (!result.clusters_path.empty() || result.neutral_nodes_path))
        throw std::invalid_argument(
            "--clusters y --neutral-nodes requieren --expanded-graph");
    return result;
}
