#include "../../lib/util/CliOptions.h"
#include <charconv>
#include <stdexcept>
#include <system_error>

CliOptions CliOptions::parse(int argc, const char* const* argv) {
    CliOptions result;

    if (argc != 4)
        throw std::invalid_argument("Uso: solverExec <input.dat> <curvas.dat> <reachability>");

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.empty())
            throw std::invalid_argument("La ruta de entrada esta vacia");
    }

    result.graph_path = argv[1];
    result.turns_path = argv[2];
    const std::string reachability = argv[3];
    const auto parsed = std::from_chars(reachability.data(),
                                        reachability.data() + reachability.size(),
                                        result.reachability);
    if (parsed.ec != std::errc() || parsed.ptr != reachability.data() + reachability.size()
        || result.reachability < 0)
        throw std::invalid_argument("reachability debe ser un entero no negativo");
    return result;
}
