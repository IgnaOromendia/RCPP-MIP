#include "../lib/CliOptions.h"
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
double number(const std::string& text, const std::string& name) {
    try {
        std::size_t end;
        const double value = std::stod(text, &end);
        if (end == text.size() && std::isfinite(value)) return value;
    } catch (const std::exception&) {}
    throw std::invalid_argument(name + ": numero invalido");
}

int integer(const std::string& text, const std::string& name) {
    try {
        std::size_t end;
        const long long value = std::stoll(text, &end);
        if (end == text.size() && value >= std::numeric_limits<int>::min() &&
            value <= std::numeric_limits<int>::max()) return static_cast<int>(value);
    } catch (const std::exception&) {}
    throw std::invalid_argument(name + ": entero invalido");
}
}

CliOptions CliOptions::parse(int argc, const char* const* argv) {
    CliOptions result;
    if (argc == 1) {
        result.help = true;
        return result;
    }
    std::vector<std::string> positional;
    bool literal = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (!literal && arg == "--") { literal = true; continue; }
        if (!literal && (arg == "--help" || arg == "-h")) {
            result.help = true;
            return result;
        }
        if (!literal && arg.rfind("--", 0) == 0) {
            if (arg != "--capacity" && arg != "--max-traversals" && arg != "--output")
                throw std::invalid_argument("Opcion desconocida: " + arg);
            if (++i >= argc) throw std::invalid_argument("Falta valor para " + arg);
            const std::string value = argv[i];
            if (arg == "--capacity") result.model.capacity = number(value, arg);
            else if (arg == "--max-traversals") result.model.max_traversals = integer(value, arg);
            else result.output_path = value;
        } else positional.push_back(arg);
    }
    if (positional.size() < 2 || positional.size() > 4)
        throw std::invalid_argument("Uso: solverExec <input.dat> <curvas.dat> [gap] [cutsMode] [opciones]");
    result.graph_path = positional[0];
    result.turns_path = positional[1];
    if (positional.size() >= 3) result.gap = number(positional[2], "gap");
    if (positional.size() >= 4) result.cuts_mode = integer(positional[3], "cutsMode");
    // CPLEX retains responsibility for the allowed gap/cutsMode ranges.
    result.model.validate();
    if (result.output_path.empty()) throw std::invalid_argument("La ruta de salida esta vacia");
    return result;
}
