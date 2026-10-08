#include <io/ClusterPathSortInstanceReader.h>
#include <io/PathOrderWriter.h>
#include <model/PathSorterClusters.h>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

int main(int argc, char** argv) {
    if (argc < 5 || argc > 7) {
        std::cerr << "Uso: pathSortClusterExec <input.dat> <curvas.dat> <solucion.dat> "
                     "<clusters.dat> [orden.dat] [route_segments_N.csv]\n";
        return 1;
    }

    try {
        ClusterPathSortInstance instance = ClusterPathSortInstanceReader::read_files(
            argv[1], argv[2], argv[3], argv[4]);
        const std::filesystem::path default_output_directory =
            std::filesystem::path("output") / "order";
        const std::string output_path = argc >= 6
            ? argv[5]
            : (default_output_directory /
               ("out_" + std::to_string(instance.path.original_nodes) + ".dat")).string();
        const std::string segments_path = argc == 7
            ? argv[6]
            : (default_output_directory /
               ("route_segments_" + std::to_string(instance.path.original_nodes) + ".csv"))
                  .string();
        const int deposit = instance.path.deposit;

        PathSorterCluster sorter(std::move(instance));
        sorter.generate_MIP();
        const CPLEXSolveResult result = sorter.solve(0.01);
        if (!result.has_solution) {
            std::cerr << "No se encontro un orden factible.\n";
            return result.status == IloAlgorithm::Error ? 1 : 2;
        }

        const std::vector<OrderedPass> order = sorter.extract_order();
        if (argc < 7) std::filesystem::create_directories(default_output_directory);
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
