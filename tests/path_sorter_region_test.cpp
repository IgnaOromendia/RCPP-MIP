#include <model/PathSorterRegion.h>
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
              "Unexpected Region diagnostic");
        return;
    }
    throw std::runtime_error("PathSorterRegion accepted an unimplemented operation");
}

static_assert(std::is_base_of_v<CPLEXSolver, PathSorterRegion>);
static_assert(!std::is_copy_constructible_v<PathSorterRegion>);
static_assert(!std::is_move_constructible_v<PathSorterRegion>);

int main() {
    try {
        PathSortInstance instance;
        instance.vehicles = 1;
        instance.edges = {{1, 0, 0, 1, 1, 1, 0, 0, 1}};

        PathSorterRegion sorter(std::move(instance));
        check(sorter.instance().vehicles == 1 && sorter.edges().size() == 1,
              "Region must retain its shared input instance");
        expect_not_implemented([&] { sorter.generate_MIP(); });
        expect_not_implemented([&] { sorter.solve(); });
        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}
