#include <util/ClusterCliOptions.h>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

ClusterCliOptions parse(std::initializer_list<const char*> arguments) {
    std::vector<const char*> argv(arguments);
    return ClusterCliOptions::parse(static_cast<int>(argv.size()), argv.data());
}

template <class Exception>
void check_rejected(std::initializer_list<const char*> arguments) {
    try {
        parse(arguments);
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error("The cluster CLI accepted invalid arguments");
}
}

int main() {
    try {
        const ClusterCliOptions defaults = parse({
            "pathSortClusterExec", "graph.dat", "turns.dat",
            "solution.dat", "clusters.dat",
        });
        check(defaults.graph_path == "graph.dat" &&
                  defaults.turns_path == "turns.dat" &&
                  defaults.solution_path == "solution.dat" &&
                  defaults.clusters_path == "clusters.dat",
              "The cluster CLI did not preserve its input paths");
        check(defaults.lookahead == 4 && defaults.branch_width == 8,
              "The cluster CLI defaults changed");
        const ClusterOutputPaths default_paths = defaults.output_paths(140);
        check(default_paths.order == "data/140/order_140.dat" &&
                  default_paths.segments == "data/140/route_segments_140.csv" &&
                  default_paths.hierholzer_segments ==
                      "data/140/route_segments_140_h.csv" &&
                  default_paths.uses_default_directory,
              "The default cluster output paths changed");

        const ClusterCliOptions explicit_options = parse({
            "pathSortClusterExec", "--lookahead", "3", "graph.dat",
            "turns.dat", "solution.dat", "clusters.dat", "order.out",
            "segments.csv", "--branch-width", "5",
        });
        check(explicit_options.lookahead == 3 &&
                  explicit_options.branch_width == 5,
              "The cluster search options were not parsed");
        const ClusterOutputPaths explicit_paths =
            explicit_options.output_paths(140);
        check(explicit_paths.order == "order.out" &&
                  explicit_paths.segments == "segments.csv" &&
                  explicit_paths.hierholzer_segments == "segments_h.csv" &&
                  !explicit_paths.uses_default_directory,
              "The explicit cluster output paths changed");

        check_rejected<ClusterCliUsageError>({"pathSortClusterExec"});
        check_rejected<ClusterCliUsageError>({
            "pathSortClusterExec", "g", "t", "s", "c", "o", "segments", "extra",
        });
        check_rejected<std::invalid_argument>({
            "pathSortClusterExec", "g", "t", "s", "c", "--lookahead", "0",
        });
        check_rejected<std::invalid_argument>({
            "pathSortClusterExec", "g", "t", "s", "c", "--branch-width",
        });
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
