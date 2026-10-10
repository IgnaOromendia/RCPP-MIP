#include <io/ClusterPathSortInstanceReader.h>
#include <io/PathOrderWriter.h>
#include <model/PathSorterClusters.h>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
std::string hierholzer_segments_path(const std::string& segments_path) {
    const std::filesystem::path path(segments_path);
    return (path.parent_path() /
            (path.stem().string() + "_h" + path.extension().string())).string();
}

struct CommandLine {
    std::vector<std::string> positional;
    int lookahead = 4;
    int branch_width = 8;
};

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

CommandLine parse_command_line(int argc, char** argv) {
    CommandLine result;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--lookahead" || argument == "--branch-width") {
            if (i + 1 >= argc)
                throw std::invalid_argument("Falta el valor de " + argument);
            const int value = positive_integer(argv[++i], argument);
            if (argument == "--lookahead") result.lookahead = value;
            else result.branch_width = value;
        } else {
            result.positional.push_back(argument);
        }
    }
    return result;
}

void print_usage() {
    std::cerr << "Uso: pathSortClusterExec <input.dat> <curvas.dat> <solucion.dat> "
                 "<clusters.dat> [orden.dat] [route_segments_N.csv] "
                 "[--lookahead N] [--branch-width N]\n";
}
}

int main(int argc, char** argv) {
    try {
        const CommandLine inputs = parse_command_line(argc, argv);
        if (inputs.positional.size() < 4 || inputs.positional.size() > 6) {
            print_usage();
            return 1;
        }
        ClusterPathSortInstance instance = ClusterPathSortInstanceReader::read_files(
            inputs.positional[0], inputs.positional[1],
            inputs.positional[2], inputs.positional[3]);
        const std::filesystem::path default_output_directory =
            std::filesystem::path("data") /
            std::to_string(instance.path.original_nodes);
        const std::string output_path = inputs.positional.size() >= 5
            ? inputs.positional[4]
            : (default_output_directory /
               ("order_" + std::to_string(instance.path.original_nodes) + ".dat")).string();
        const std::string segments_path = inputs.positional.size() == 6
            ? inputs.positional[5]
            : (default_output_directory /
               ("route_segments_" + std::to_string(instance.path.original_nodes) + ".csv"))
                  .string();
        const int deposit = instance.path.deposit;

        PathSorterCluster sorter(
            std::move(instance), inputs.lookahead, inputs.branch_width);
        if (inputs.positional.size() < 6)
            std::filesystem::create_directories(default_output_directory);
        const std::string hierholzer_path =
            hierholzer_segments_path(segments_path);
        PathOrderWriter::write_cluster_segments_file(
            hierholzer_path, sorter.hierholzer_order(), sorter.edge_clusters());
        std::cout << "Segmentos de Hierholzer guardados en "
                  << hierholzer_path << " (lookahead=" << inputs.lookahead
                  << ", ancho=" << inputs.branch_width << ")\n";

        sorter.generate_MIP();
        sorter.set_time_limit(600);
        const CPLEXSolveResult result = sorter.solve(0.1);
        if (!result.has_solution) {
            std::cerr << "No se encontro un orden factible.\n";
            return result.status == IloAlgorithm::Error ? 1 : 2;
        }
        std::cout << "Gap: " << result.relative_gap * 100.0 << "%\n";

        const std::vector<OrderedPass> order = sorter.extract_order();
        PathOrderWriter::write_file(output_path, order, deposit);
        PathOrderWriter::write_cluster_segments_file(
            segments_path, order, sorter.edge_clusters());
        std::cout << "Orden por clusters guardado en " << output_path << '\n';
        std::cout << "Segmentos guardados en " << segments_path << '\n';
        return 0;
    } catch (const IloException& error) {
        std::cerr << "Error de CPLEX: " << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
    }
    return 1;
}
