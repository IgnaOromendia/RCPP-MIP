#include <model/PathSorterFirstPass.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

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

struct PathSorterFirstPassTestAccess {
    static const std::vector<segment>& segments(const PathSorterFirstPass& sorter) {
        return sorter._segments;
    }

    static const VariableMatrix& transitions(const PathSorterFirstPass& sorter) {
        return sorter._Z;
    }
};

PathSortInstance repeated_route() {
    PathSortInstance instance;
    instance.vehicles = 1;
    instance.deposit = 3;
    instance.edges = {
        {10, -2, 3, 0, 1, 0, 1},
        {11, 0, 0, 1, 1, 1, 1, 0, 1},
        {12, 1, 1, 0, 1, 0, 1, 1, 0},
        {13, -2, 1, 3, 1, 0, 1}
    };
    instance.adj.resize(4);
    instance.adj[3] = {{0, 10}};
    instance.adj[0] = {{1, 11}};
    instance.adj[1] = {{0, 12}, {3, 13}};
    return instance;
}

void check_repeated_route_and_returned_order() {
    PathSorterFirstPass sorter(repeated_route());
    check(sorter.total_passes() == 5, "K must include every concrete pass");
    check(PathSorterFirstPassTestAccess::segments(sorter) ==
              std::vector<segment>({{0, 0}, {1, 0}, {1, 1}, {2, 0}, {3, 0}}),
          "Concrete passes must retain their edge and occurrence indexes");

    expect_throws<std::logic_error>([&] { sorter.extract_order(); },
                                    "No hay una solucion");
    sorter.generate_MIP();

    const VariableMatrix& Z = PathSorterFirstPassTestAccess::transitions(sorter);
    check(Z[0][1].getLB() == 0 && Z[0][1].getUB() == 1,
          "A compatible transition must remain selectable");
    check(Z[0][3].getLB() == 0 && Z[0][3].getUB() == 0,
          "An impossible transition must be fixed to zero");
    check(Z[4][0].getUB() == 0,
          "The arrival pass must not return to the departure pass");

    const CPLEXSolveResult solved = sorter.solve(0);
    check(solved.has_solution, "The repeated-pass route must be feasible");
    const std::vector<OrderedPass> order = sorter.extract_order();
    check(order.size() == 5, "The returned order must contain every pass");
    for (std::size_t i = 0; i < order.size(); ++i)
        check(order[i].position == static_cast<int>(i + 1),
              "Returned positions must be consecutive");
    check(order.front().edge.from == 3 && order.back().edge.to == 3,
          "The returned route must start and finish at the deposit");
    for (std::size_t i = 0; i + 1 < order.size(); ++i)
        check(order[i].edge.to == order[i + 1].edge.from,
              "The returned order must be continuous");

    std::vector<int> repeated_passes;
    for (const OrderedPass& item : order)
        if (item.edge.super_arc_id == 11) repeated_passes.push_back(item.pass);
    check(repeated_passes == std::vector<int>({1, 2}),
          "Repeated passes must be returned in occurrence order");
    check(std::isfinite(sorter.minimum_distance()),
          "A solved model must expose its objective");
}

void check_invalid_routes() {
    PathSortInstance missing_arrival = repeated_route();
    missing_arrival.edges.pop_back();
    PathSorterFirstPass without_arrival(std::move(missing_arrival));
    expect_throws<std::invalid_argument>(
        [&] { without_arrival.generate_MIP(); }, "salir y regresar");

    PathSortInstance duplicate_departure = repeated_route();
    duplicate_departure.edges.push_back({14, -2, 3, 0, 1, 0, 1});
    PathSorterFirstPass with_duplicate(std::move(duplicate_departure));
    expect_throws<std::invalid_argument>(
        [&] { with_duplicate.generate_MIP(); }, "mas de una pasada que sale");

    PathSortInstance disconnected = repeated_route();
    disconnected.edges[1].from = 2;
    PathSorterFirstPass without_transition(std::move(disconnected));
    expect_throws<std::invalid_argument>(
        [&] { without_transition.generate_MIP(); }, "sucesora compatible");

}

int main() {
    try {
        check_repeated_route_and_returned_order();
        check_invalid_routes();
        return 0;
    } catch (const IloException& error) {
        std::cerr << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
