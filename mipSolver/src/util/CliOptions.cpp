#include "../../lib/util/CliOptions.h"
#include <charconv>
#include <stdexcept>
#include <system_error>

CliOptions CliOptions::parse(int argc, const char* const* argv) {
    CliOptions result;

    if (argc < 3 || argc > 5)
        throw std::invalid_argument(
            "Uso: solverExec <input.dat> <curvas.dat> [mip|fixAndOptimize <reachability>]");

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
            throw std::invalid_argument("mip no recibe reachability");
        return result;
    }
    if (strategy != "fixAndOptimize")
        throw std::invalid_argument("strategy debe ser 'mip' o 'fixAndOptimize'");
    if (argc != 5)
        throw std::invalid_argument("fixAndOptimize requiere reachability");

    result.strategy = SolverStrategy::FixAndOptimize;
    const std::string reachability = argv[4];
    const auto parsed = std::from_chars(reachability.data(),
                                        reachability.data() + reachability.size(),
                                        result.reachability);
    if (parsed.ec != std::errc() || parsed.ptr != reachability.data() + reachability.size()
        || result.reachability < 0)
        throw std::invalid_argument("reachability debe ser un entero no negativo");
    return result;
}
