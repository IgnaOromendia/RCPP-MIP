#include <io/ClusterPathSortInstanceReader.h>
#include <io/PathOrderWriter.h>
#include <model/PathSorterClusters.h>
#include <util/ClusterCliOptions.h>
#include <exception>
#include <filesystem>
#include <iostream>
#include <utility>
#include <vector>

int main(int argc, char** argv) {
    try {
        const ClusterCliOptions inputs = ClusterCliOptions::parse(argc, argv);
        ClusterPathSortInstance instance = ClusterPathSortInstanceReader::read_files(inputs);
        const ClusterOutputPaths paths = inputs.output_paths(instance.path.original_nodes);
        const int deposit = instance.path.deposit;

        PathSorterCluster sorter(std::move(instance), inputs.lookahead, inputs.branch_width);

        if (paths.uses_default_directory)
            std::filesystem::create_directories(paths.directory);

        // Warm start
        std::vector<OrderedPass> hierholzer_order = sorter.hierholzer_order();
        PathOrderWriter::write_cluster_segments_file(paths.hierholzer_segments, hierholzer_order, sorter.edge_clusters());
        std::cout << "Segmentos de Hierholzer guardados en "
                  << paths.hierholzer_segments
                  << " (lookahead=" << inputs.lookahead
                  << ", ancho=" << inputs.branch_width << ")\n";

        sorter.generate_MIP();
        if (sorter.complete_hierholzer_warm_start())
            std::cout << "CPLEX completo el incumbente de Hierholzer "
                         "resolviendo el modelo restringido.\n";
        else
            std::cerr << "CPLEX no pudo completar el incumbente de "
                         "Hierholzer con el modelo restringido.\n";

        sorter.set_time_limit(1200);
        const CPLEXSolveResult result = sorter.solve(0.1);
        std::vector<OrderedPass> order;
        
        if (result.has_solution) {
            std::cout << "Gap: " << result.relative_gap * 100.0 << "%\n";
            order = sorter.extract_order();
        } else {
            std::cerr << "CPLEX no obtuvo un orden optimizado; se usa el orden factible de Hierholzer.\n";
            order = std::move(hierholzer_order);
        }

        PathOrderWriter::write_file(paths.order, order, deposit);
        PathOrderWriter::write_cluster_segments_file(paths.segments, order, sorter.edge_clusters());
        std::cout << (result.has_solution ? "Orden por clusters guardado en " : "Orden de Hierholzer guardado en ")
                  << paths.order << '\n';
        std::cout << "Segmentos guardados en " << paths.segments << '\n';
        return 0;
    } catch (const ClusterCliUsageError& error) {
        std::cerr << error.what() << '\n';
    } catch (const IloException& error) {
        std::cerr << "Error de CPLEX: " << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
    }
    return 1;
}
