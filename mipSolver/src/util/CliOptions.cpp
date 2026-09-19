#include "../../lib/util/CliOptions.h"
#include <stdexcept>

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
    result.reachability = atoi(argv[3]);
    return result;
}
