#include <util/ClusterCliOptions.h>
#include <vector>

namespace {
int positive_integer(const std::string& value, const std::string& option) {
    std::size_t parsed = 0;
    int result = 0;
    try {
        result = std::stoi(value, &parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument(option + " requiere un entero positivo");
    }
    if (parsed != value.size() || result <= 0)
        throw std::invalid_argument(option + " requiere un entero positivo");
    return result;
}

std::string hierholzer_path(const std::string& segments_path) {
    const std::filesystem::path path(segments_path);
    return (path.parent_path() /
            (path.stem().string() + "_h" + path.extension().string())).string();
}
}

const char* ClusterCliOptions::usage() {
    return "Uso: pathSortClusterExec <input.dat> <curvas.dat> <solucion.dat> "
           "<clusters.dat> [orden.dat] [route_segments_N.csv] "
           "[--lookahead N] [--branch-width N]";
}

ClusterCliOptions ClusterCliOptions::parse(
    int argc,
    const char* const* argv) {
    ClusterCliOptions result;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--lookahead" || argument == "--branch-width") {
            if (i + 1 >= argc)
                throw std::invalid_argument("Falta el valor de " + argument);
            const int value = positive_integer(argv[++i], argument);
            if (argument == "--lookahead") result.lookahead = value;
            else result.branch_width = value;
        } else {
            positional.push_back(argument);
        }
    }

    if (positional.size() < 4 || positional.size() > 6)
        throw ClusterCliUsageError(usage());

    result.graph_path = positional[0];
    result.turns_path = positional[1];
    result.solution_path = positional[2];
    result.clusters_path = positional[3];
    if (positional.size() >= 5) result.order_path = positional[4];
    if (positional.size() == 6) result.segments_path = positional[5];
    return result;
}

ClusterOutputPaths ClusterCliOptions::output_paths(int original_nodes) const {
    const std::filesystem::path directory =
        std::filesystem::path("data") / std::to_string(original_nodes);
    const std::string order = order_path.value_or(
        (directory / ("order_" + std::to_string(original_nodes) + ".dat")).string());
    const std::string segments = segments_path.value_or(
        (directory /
         ("route_segments_" + std::to_string(original_nodes) + ".csv")).string());
    return {
        directory,
        order,
        segments,
        hierholzer_path(segments),
        !segments_path.has_value(),
    };
}
