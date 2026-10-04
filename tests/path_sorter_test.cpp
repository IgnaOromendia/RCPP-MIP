#include <model/PathSorter.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        PathSortInstance instance;
        instance.vehicles = 2;
        instance.edges = {
            {3, 0, 4, 5, 1, 1, 2},
            {7, -1, 5, 8, 2, 0, 1}
        };

        PathSorter sorter(std::move(instance));
        check(sorter.edges().size() == 2, "PathSorter must retain every input edge");
        check(sorter.edges()[0].super_arc_id == 3 &&
              sorter.edges()[0].original_edge_id == 0 &&
              sorter.edges()[0].vehicle == 1 &&
              sorter.edges()[0].times() == 3,
              "PathSorter must consume the already parsed multiplicities");
        check(sorter.edges()[1].original_edge_id == -1 && sorter.edges()[1].times() == 1,
              "PathSorter must retain connector edges");
        check(sorter.pass_counts() == std::vector<int>({3, 1}),
              "PathSorter must define m_e from the incumbent traversal counts");
        check(sorter.total_passes() == 4,
              "PathSorter must define K as the sum of every m_e");

        // Sparse super-arc ids must not be used as indexes into the two local edges.
        sorter.generate_MIP();

        PathSorter empty;
        check(empty.edges().empty(), "Default PathSorter must have no input edges");
        check(empty.pass_counts().empty() && empty.total_passes() == 0,
              "An empty sorter must define an empty set of passes");
        empty.generate_MIP();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
