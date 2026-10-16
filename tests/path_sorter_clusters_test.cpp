#include <cmath>
#include <heuristic/ClusterGuidedHierholzer.h>
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
#include <vector>

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

    static const std::vector<int>& warm_start_order(
        const PathSorterCluster& sorter) {
        return sorter._warm_start_order;
    }

    static int mip_start_count(const PathSorterCluster& sorter) {
        return sorter.get_mip_start_count();
    }

    static std::string mip_start_name(PathSorterCluster& sorter, int index) {
        return sorter.get_mip_start_name(index);
    }

    static double time_limit(const PathSorterCluster& sorter) {
        return sorter._solver.getParam(IloCplex::Param::TimeLimit);
    }

    static void check_route_bounds_restored(const PathSorterCluster& sorter) {
        for (int r = 0; r < sorter._K; ++r) {
            check(sorter._X[r].getLB() == 1 &&
                      sorter._X[r].getUB() == sorter._K,
                  "Restricted solve must restore every X bound");
            for (int s = 0; s < sorter._K; ++s)
                if (sorter.is_transition(r, s))
                    check(sorter._Z[r][s].getLB() == 0 &&
                              sorter._Z[r][s].getUB() == 1,
                          "Restricted solve must restore every Z bound");
        }
    }
};

template <class Exception, class Function>
void expect_throws(Function function, const std::string& fragment) {
    try {
        function();
    } catch (const Exception& error) {
        check(std::string(error.what()).find(fragment) != std::string::npos,
              "Unexpected exception: " + std::string(error.what()));
        return;
    }
    throw std::runtime_error("Expected exception containing: " + fragment);
}

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
    // route, their clusters are ordered 0, 0, 6, 0, 6, 6, so exactly three
    // consecutive transitions cross from one cluster to another.
    return {
        std::move(path),
        {0, 0, 6, 0, 6, 6},
        {{0, {0, 1, 3}}, {6, {2, 4, 5}}},
        7
    };
}

ClusterPathSortInstance cluster_guided_route() {
    PathSortInstance path;
    path.vehicles = 1;
    path.deposit = 3;
    path.edges = {
        {10, -2, 3, 0, 1, 0, 1},
        {11, 0, 0, 1, 1, 1, 0, 0, 1},
        {12, 1, 1, 0, 1, 1, 0, 1, 0},
        {13, 2, 0, 2, 1, 1, 0, 0, 2},
        {14, 3, 2, 0, 1, 1, 0, 2, 0},
        {15, -2, 0, 3, 1, 0, 1}
    };
    return {
        std::move(path),
        {0, 0, 0, 1, 1, 2},
        {{0, {0, 1, 2}}, {1, {3, 4}}, {2, {5}}},
        3
    };
}

ClusterPathSortInstance repeated_cluster_route() {
    PathSortInstance path;
    path.vehicles = 1;
    path.deposit = 3;
    path.edges = {
        {20, -2, 3, 0, 1, 0, 1},
        {21, 0, 0, 1, 1, 1, 1, 0, 1},
        {22, 1, 1, 0, 1, 0, 1, 1, 0},
        {23, -2, 1, 3, 1, 0, 1}
    };
    return {
        std::move(path),
        {0, 0, 0, 1},
        {{0, {0, 1, 2}}, {1, {3}}},
        2
    };
}

ClusterPathSortInstance lookahead_cluster_route() {
    PathSortInstance path;
    path.vehicles = 1;
    path.deposit = 5;
    path.edges = {
        {30, -2, 5, 0, 1, 0, 1},
        {31, 0, 0, 1, 1, 1, 0, 0, 1},
        {32, 1, 1, 2, 1, 1, 0, 1, 2},
        {33, 2, 2, 0, 1, 1, 0, 2, 0},
        {34, 3, 0, 3, 1, 1, 0, 0, 3},
        {35, 4, 3, 4, 1, 1, 0, 3, 4},
        {36, 5, 4, 0, 1, 1, 0, 4, 0},
        {37, -2, 0, 5, 1, 0, 1}
    };
    return {
        std::move(path),
        {0, 0, 1, 0, 1, 1, 1, 2},
        {{0, {0, 1, 3}}, {1, {2, 4, 5, 6}}, {2, {7}}},
        3
    };
}

void check_hierholzer_lookahead() {
    PathSorterCluster local(lookahead_cluster_route(), 1);
    check(PathSorterClusterTestAccess::warm_start_order(local) ==
              std::vector<int>({0, 1, 2, 3, 4, 5, 6, 7}),
          "Depth one must preserve the local cluster decision");

    PathSorterCluster lookahead(lookahead_cluster_route(), 3);
    check(PathSorterClusterTestAccess::warm_start_order(lookahead) ==
              std::vector<int>({0, 4, 5, 6, 1, 2, 3, 7}),
          "Lookahead must avoid the locally attractive cluster reopening");
    expect_throws<std::invalid_argument>(
        [&] { PathSorterCluster invalid(lookahead_cluster_route(), 0); },
        "lookahead");
}

void check_hierholzer_warm_start() {
    PathSorterCluster sorter(cluster_guided_route());
    const std::vector<int>& order =
        PathSorterClusterTestAccess::warm_start_order(sorter);
    check(order == std::vector<int>({0, 1, 2, 3, 4, 5}),
          "Hierholzer must prefer the available edge in the previous cluster");
    check(order.size() == 6 && sorter.total_passes() == 6,
          "Hierholzer must consume every passage");

    const std::vector<PathEdge>& edges = sorter.edges();
    for (std::size_t i = 0; i + 1 < order.size(); ++i)
        check(edges[order[i]].to == edges[order[i + 1]].from,
              "The warm-start circuit must be continuous");
    check(edges[order.front()].from == sorter.instance().deposit &&
              edges[order.back()].to == sorter.instance().deposit,
          "The warm-start circuit must start and finish at the deposit");

    sorter.generate_MIP();
    check(PathSorterClusterTestAccess::mip_start_count(sorter) == 1,
          "The cluster model must register exactly one MIP start");
    check(PathSorterClusterTestAccess::mip_start_name(sorter, 0) ==
              "cluster-hierholzer",
          "The Hierholzer MIP start must have a stable name");
    sorter.set_time_limit(5);
    check(sorter.complete_hierholzer_warm_start(),
          "The restricted model must complete the Hierholzer incumbent");
    check(PathSorterClusterTestAccess::time_limit(sorter) == 5,
          "Completing the warm start must preserve the configured time limit");
    check(PathSorterClusterTestAccess::mip_start_count(sorter) == 1,
          "The complete incumbent must replace the partial MIP start");
    check(PathSorterClusterTestAccess::mip_start_name(sorter, 0) ==
              "cluster-hierholzer-restricted-solve",
          "The complete incumbent must have a stable name");
    PathSorterClusterTestAccess::check_route_bounds_restored(sorter);
    const CPLEXSolveResult solved = sorter.solve(0);
    check(solved.has_solution,
          "CPLEX must accept the completed Hierholzer incumbent");
    check(PathSorterClusterTestAccess::mip_start_count(sorter) == 1,
          "Solving must not discard the completed MIP start");
}

void check_repeated_passages_in_warm_start() {
    PathSorterCluster sorter(repeated_cluster_route());
    check(PathSorterClusterTestAccess::warm_start_order(sorter) ==
              std::vector<int>({0, 1, 3, 2, 4}),
          "Repeated edges must map to segments in occurrence order");
    sorter.generate_MIP();
    const CPLEXSolveResult solved = sorter.solve(0);
    check(solved.has_solution,
          "The repeated-passage warm start must be feasible");
}

void check_non_eulerian_diagnostic() {
    PathSortInstance path;
    path.vehicles = 1;
    path.deposit = 3;
    path.edges = {
        {30, -2, 3, 0, 1, 0, 1},
        {31, -2, 0, 3, 1, 0, 1},
        {32, 0, 1, 2, 1, 1, 0},
        {33, 1, 2, 1, 1, 1, 0}
    };
    ClusterPathSortInstance disconnected{
        std::move(path), {0, 0, 0, 0}, {{0, {0, 1, 2, 3}}}, 1};
    expect_throws<std::invalid_argument>(
        [&] { PathSorterCluster sorter(std::move(disconnected)); },
        "circuito euleriano");
}

void check_sparse_cluster_constraints() {
    PathSorterCluster sorter(forced_mixed_route());
    sorter.generate_MIP();

    const CPLEXSolveResult solved = sorter.solve(0);
    check(solved.has_solution && solved.status == IloAlgorithm::Optimal,
          "The sparse-cluster route must have an optimal solution");

    const std::vector<OrderedPass> order = sorter.extract_order();
    check(order.size() == 6, "The cluster model must order every pass");
    int expected_position = 1;
    for (const OrderedPass& pass : order) {
        check(pass.position == expected_position,
              "Cluster positions must be consecutive");
        if (expected_position < order.size())
            check(pass.edge.to == order[expected_position].edge.from,
                  "The cluster order must be continuous");
        ++expected_position;
    }

    check(std::abs(PathSorterClusterTestAccess::objective(sorter) - 3.0) < 1e-6,
          "The mixed route must count its three cross-cluster transitions");
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
            {3, -2, 1, 6, 1, 0, 1, -1, -1},
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
            "3 7 2 D 1 0 1 1\n";
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
            {3, -2, 1, 6, 1, 0, 1, -1, -1},
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
        check_hierholzer_warm_start();
        check_hierholzer_lookahead();
        check_repeated_passages_in_warm_start();
        check_non_eulerian_diagnostic();
        check_sparse_cluster_constraints();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
