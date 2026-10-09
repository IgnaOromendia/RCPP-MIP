#include <cmath>
#include <constraints/ClusterConstraintSetter.h>
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

    static const IloNumVarArray& cluster_max_positions(
        const PathSorterCluster& sorter) {
        return sorter._L;
    }
};

using Terms = std::map<IloInt, double>;

struct ExpectedRow {
    double lower;
    double upper;
    Terms terms;

    void add(IloNumVar variable, double coefficient) {
        terms[variable.getId()] += coefficient;
    }
};

void expect_rows(IloModel model, std::map<std::string, ExpectedRow> expected) {
    for (IloModel::Iterator it(model); it.ok(); ++it) {
        auto* implementation = dynamic_cast<IloRangeI*>((*it).getImpl());
        check(implementation != nullptr,
              "Cluster setter added an object other than a range");
        IloRange range(implementation);
        const std::string name = range.getName() ? range.getName() : "<unnamed>";
        auto found = expected.find(name);
        check(found != expected.end(), "Unexpected or duplicate row: " + name);
        check(range.getLB() == found->second.lower &&
                  range.getUB() == found->second.upper,
              "Incorrect bounds: " + name);

        Terms actual;
        for (IloExpr::LinearIterator term = range.getLinearIterator();
             term.ok(); ++term)
            if (term.getCoef() != 0)
                actual[term.getVar().getId()] += term.getCoef();
        check(actual == found->second.terms,
              "Incorrect variables or coefficients: " + name);
        expected.erase(found);
    }
    check(expected.empty(),
          "Missing row: " + (expected.empty() ? "" : expected.begin()->first));
}

struct Environment {
    IloEnv env;
    ~Environment() { env.end(); }
};

void check_cluster_constraint_rows() {
    Environment environment;
    IloEnv& env = environment.env;
    IloModel model(env);
    constexpr int cluster_count = 3;
    constexpr int edge_count = 3;
    constexpr int K = 5;

    IloNumVarArray X(env, edge_count, 1, K, ILOINT);
    IloNumVarArray L(env, cluster_count, 1, K, ILOFLOAT);
    VariableMatrix O(env, cluster_count), Q(env, cluster_count);
    for (int c = 0; c < cluster_count; ++c) {
        O[c] = IloNumVarArray(env, cluster_count, 0, 1, ILOBOOL);
        Q[c] = IloNumVarArray(env, edge_count, 0, 1, ILOBOOL);
    }

    const std::map<int, std::vector<int>> edges_by_cluster = {
        {0, {0, 2}}, {2, {1}}
    };
    const std::map<segment, int> segment_map = {
        {{0, 0}, 0}, {{1, 0}, 1}, {{2, 0}, 2}
    };

    ClusterConstraintSetter setter(
        O, Q, X, L, edges_by_cluster, segment_map,
        cluster_count, K, env, model);
    setter.set_cluster_max_position_constraint();
    setter.set_cluster_constraint();

    std::map<std::string, ExpectedRow> expected;
    for (int e : {0, 2}) {
        ExpectedRow row{0, IloInfinity, {}};
        row.add(L[0], 1);
        row.add(X[e], -1);
        expected.emplace("Cluster_2_0_" + std::to_string(e), std::move(row));
    }
    ExpectedRow max_cluster_2{0, IloInfinity, {}};
    max_cluster_2.add(L[2], 1);
    max_cluster_2.add(X[1], -1);
    expected.emplace("Cluster_2_2_1", std::move(max_cluster_2));

    ExpectedRow direction_0_2{-K, IloInfinity, {}};
    direction_0_2.add(Q[0][1], K);
    direction_0_2.add(O[0][2], -K);
    direction_0_2.add(L[0], -1);
    direction_0_2.add(X[1], 1);
    expected.emplace("Cluster_3_0_2_1", std::move(direction_0_2));

    for (int e : {0, 2}) {
        ExpectedRow row{0, IloInfinity, {}};
        row.add(Q[2][e], K);
        row.add(O[0][2], K);
        row.add(L[2], -1);
        row.add(X[e], 1);
        expected.emplace("Cluster_4_0_2_" + std::to_string(e), std::move(row));
    }

    expect_rows(model, std::move(expected));
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

void check_sparse_cluster_constraints() {
    PathSorterCluster sorter(forced_mixed_route());
    sorter.generate_MIP();

    const IloNumVarArray& L =
        PathSorterClusterTestAccess::cluster_max_positions(sorter);
    check(L.getSize() == 7, "L must have one entry per declared cluster id");
    for (IloInt c = 0; c < L.getSize(); ++c) {
        check(L[c].getLB() == 1 && L[c].getUB() == 6,
              "Every L variable must use the documented [1,K] bounds");
        check(L[c].getType() == IloNumVar::Float,
              "L must be continuous as documented");
    }

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
          "The mixed route must penalize the one early edge with pair weight 1/6");
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
        check_cluster_constraint_rows();
        check_sparse_cluster_constraints();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
