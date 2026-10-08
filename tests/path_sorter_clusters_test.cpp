#include <cmath>
#include <model/PathSorterClusters.h>
#include <io/ClusterPathSortInstanceReader.h>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

static_assert(std::is_base_of_v<CPLEXSolver, PathSorterCluster>);
static_assert(std::is_base_of_v<PathSolver, PathSorterCluster>);
static_assert(!std::is_copy_constructible_v<PathSorterCluster>);
static_assert(!std::is_move_constructible_v<PathSorterCluster>);

struct PathSorterClusterTestAccess {
    static double objective(const PathSorterCluster& sorter) {
        return sorter.get_objective_value();
    }

    static double cluster_order(const PathSorterCluster& sorter, int c, int d) {
        return sorter.get_value(sorter._O[c][d]);
    }

    static double early_edge(const PathSorterCluster& sorter, int c, int e) {
        return sorter.get_value(sorter._Q[c][e]);
    }
};

ClusterPathSortInstance forced_mixed_route() {
    PathSortInstance path;
    path.vehicles = 1;
    path.deposit = 5;
    path.edges = {
        {10, -2, 5, 0, 1, 0, 1},
        {11, 0, 0, 1, 1, 1, 0, 0, 1},
        {12, 1, 1, 2, 1, 1, 0, 1, 2},
        {13, 2, 2, 3, 1, 1, 0, 2, 3},
        {14, 3, 3, 4, 1, 1, 0, 3, 4},
        {15, -2, 4, 5, 1, 0, 1}
    };

    // The used identifiers are deliberately sparse. Along the only possible
    // route, their first passes are ordered 0, 0, 6, 0, 6, 6. Therefore, if
    // cluster 0 finishes first, only edge 2 from cluster 6 is early. The
    // normalized objective is 1 / (3 + 3) = 1/6.
    return {
        std::move(path),
        {0, 0, 6, 0, 6, 6},
        {{0, {0, 1, 3}}, {6, {2, 4, 5}}},
        7
    };
}

void check_sparse_cluster_constraints() {
    PathSorterCluster sorter(forced_mixed_route());
    sorter.generate_MIP();
    const CPLEXSolveResult solved = sorter.solve(0);
    check(solved.has_solution && solved.status == IloAlgorithm::Optimal,
          "The sparse-cluster route must have an optimal solution");

    const std::vector<OrderedPass> order = sorter.extract_order();
    check(order.size() == 6, "The cluster model must order every pass");
    for (std::size_t i = 0; i < order.size(); ++i) {
        check(order[i].position == static_cast<int>(i + 1),
              "Cluster positions must be consecutive");
        if (i + 1 < order.size())
            check(order[i].edge.to == order[i + 1].edge.from,
                  "The cluster order must be continuous");
    }

    check(std::abs(PathSorterClusterTestAccess::objective(sorter) - 1.0 / 6.0) < 1e-6,
          "The mixed route must penalize exactly one of six clustered edges");
    check(PathSorterClusterTestAccess::cluster_order(sorter, 0, 6) > 0.5,
          "The model must choose cluster 0 before cluster 6");
    check(PathSorterClusterTestAccess::early_edge(sorter, 0, 2) > 0.5,
          "The first edge from cluster 6 must be marked early");
    check(PathSorterClusterTestAccess::early_edge(sorter, 0, 4) < 0.5 &&
              PathSorterClusterTestAccess::early_edge(sorter, 0, 5) < 0.5,
          "Edges after cluster 0 finishes must not be marked early");
    for (int edge : {0, 1, 3})
        check(PathSorterClusterTestAccess::early_edge(sorter, 6, edge) < 0.5,
              "Inactive reverse-order penalties must remain zero");
}

int main(int argc, char* argv[]) {
    try {
        if (argc == 5) {
            ClusterPathSortInstance actual = ClusterPathSortInstanceReader::read_files(
                argv[1], argv[2], argv[3], argv[4]);
            check(!actual.path.edges.empty() &&
                  actual.edge_clusters.size() == actual.path.edges.size(),
                  "Real cluster vector must align with every parsed path edge");
            check(!actual.edges_by_cluster.empty(),
                  "Real cluster map must group parsed path edges");
            const int actual_cluster_count = actual.cluster_count;
            PathSorterCluster actual_sorter(std::move(actual));
            check(actual_sorter.edge_clusters().size() == actual_sorter.edges().size(),
                  "Cluster model lost real file assignments");
            check(actual_sorter.cluster_count() == actual_cluster_count,
                  "Cluster model lost the generated cluster count");
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
            "1 7 1 2 1 1 0 1\n"
            "2 1 D 1 1 0 1 1\n"
            "3 7 3 4 1 0 2 2\n";
        std::istringstream cluster_stream(cluster_text);
        ClusterPathSortInstance parsed = ClusterPathSortInstanceReader::read(
            std::move(instance), cluster_stream, "test clusters");
        check(parsed.edge_clusters == std::vector<int>({6, 0, 6}),
              "Clusters must align with PathSortInstance::edges, not file row order");
        check(parsed.edges_by_cluster ==
                  std::map<int, std::vector<int>>({{0, {1}}, {6, {0, 2}}}),
              "Cluster map must group PathSortInstance edge indices");
        check(parsed.cluster_count == 7,
              "Cluster count must be initialized from generated identifiers");

        PathSorterCluster sorter(std::move(parsed));
        check(sorter.instance().vehicles == 1 && sorter.edges().size() == 3,
              "Cluster must retain its shared input instance");
        check(sorter.edge_clusters() == std::vector<int>({6, 0, 6}),
              "Cluster model must retain one cluster number per edge");
        check(sorter.cluster_count() == 7,
              "Cluster model must initialize the generated cluster count");

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
        check(from_file.edge_clusters == std::vector<int>({6, 0, 6}),
              "Cluster file reader must retain aligned assignments");
        check(from_file.edges_by_cluster ==
                  std::map<int, std::vector<int>>({{0, {1}}, {6, {0, 2}}}),
              "Cluster file reader must retain grouped edge indices");
        check(from_file.cluster_count == 7,
              "Cluster file reader must retain the generated cluster count");

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
        check_sparse_cluster_constraints();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
