#include <model/PathSorterClusters.h>
#include <io/ClusterPathSortInstanceReader.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template <class Function>
void expect_not_implemented(Function function) {
    try {
        function();
    } catch (const std::logic_error& error) {
        check(std::string(error.what()).find("todavia no esta implementado") !=
                  std::string::npos,
              "Unexpected Cluster diagnostic");
        return;
    }
    throw std::runtime_error("PathSorterCluster accepted an unimplemented operation");
}

static_assert(std::is_base_of_v<CPLEXSolver, PathSorterCluster>);
static_assert(std::is_base_of_v<PathSolver, PathSorterCluster>);
static_assert(!std::is_copy_constructible_v<PathSorterCluster>);
static_assert(!std::is_move_constructible_v<PathSorterCluster>);

int main(int argc, char* argv[]) {
    try {
        if (argc == 5) {
            ClusterPathSortInstance actual = ClusterPathSortInstanceReader::read_files(
                argv[1], argv[2], argv[3], argv[4]);
            check(!actual.path.edges.empty() &&
                  actual.edge_clusters.size() == actual.path.edges.size(),
                  "Real cluster vector must align with every parsed path edge");
            PathSorterCluster actual_sorter(std::move(actual));
            check(actual_sorter.edge_clusters().size() == actual_sorter.edges().size(),
                  "Cluster model lost real file assignments");
            return 0;
        }
        check(argc == 1, "Expected either zero or four file arguments");

        PathSortInstance instance;
        instance.vehicles = 1;
        instance.deposit = 6;
        // Deliberately different from the row order in the cluster file.
        instance.edges = {
            {3, 2, 2, 3, 1, 0, 2, 2, 3},
            {4, -2, 6, 0, 1, 0, 1, -1, -1},
            {1, 0, 0, 1, 1, 1, 0, 0, 1}
        };

        const std::string cluster_text =
            "# porcentaje_objetivo 30\n"
            "# aristas_activas 3\n"
            "# max_aristas_por_cluster 1\n"
            "arista cluster origen destino vehiculo servicio recorridos pasadas\n"
            "1 4 1 2 1 1 0 1\n"
            "2 2 D 1 1 0 1 1\n"
            "3 7 3 4 1 0 2 2\n";
        std::istringstream cluster_stream(cluster_text);
        ClusterPathSortInstance parsed = ClusterPathSortInstanceReader::read(
            std::move(instance), cluster_stream, "test clusters");
        check(parsed.edge_clusters == std::vector<int>({7, 2, 4}),
              "Clusters must align with PathSortInstance::edges, not file row order");

        PathSorterCluster sorter(std::move(parsed));
        check(sorter.instance().vehicles == 1 && sorter.edges().size() == 3,
              "Cluster must retain its shared input instance");
        check(sorter.edge_clusters() == std::vector<int>({7, 2, 4}),
              "Cluster model must retain one cluster number per edge");
        expect_not_implemented([&] { sorter.generate_MIP(); });
        expect_not_implemented([&] { sorter.solve(); });

        std::ofstream cluster_file("clusters-test.dat");
        cluster_file << cluster_text;
        cluster_file.close();
        PathSortInstance file_instance;
        file_instance.deposit = 6;
        file_instance.edges = {
            {3, 2, 2, 3, 1, 0, 2, 2, 3},
            {4, -2, 6, 0, 1, 0, 1, -1, -1},
            {1, 0, 0, 1, 1, 1, 0, 0, 1}
        };
        const ClusterPathSortInstance from_file =
            ClusterPathSortInstanceReader::read_file(
                std::move(file_instance), "clusters-test.dat");
        check(from_file.edge_clusters == std::vector<int>({7, 2, 4}),
              "Cluster file reader must retain aligned assignments");

        PathSortInstance invalid_instance = from_file.path;
        std::istringstream missing(
            "arista cluster origen destino vehiculo servicio recorridos pasadas\n"
            "1 4 1 2 1 1 0 1\n");
        bool caught = false;
        try {
            ClusterPathSortInstanceReader::read(
                std::move(invalid_instance), missing, "missing clusters");
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        check(caught, "Missing cluster assignments must be rejected");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
