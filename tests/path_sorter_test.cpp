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

        PathSorter empty;
        check(empty.edges().empty(), "Default PathSorter must have no input edges");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
