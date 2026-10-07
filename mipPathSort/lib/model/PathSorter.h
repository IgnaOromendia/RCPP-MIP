#ifndef PATH_SORTER_H
#define PATH_SORTER_H

#include <CPLEXSolver.h>
#include "PathSortInstance.h"
#include <cstddef>

using namespace std;

struct OrderedPass {
    int position = 0;
    int pass = 0;
    PathEdge edge;
};

class PathSorter : public CPLEXSolver {
public:
    PathSorter();
    explicit PathSorter(PathSortInstance instance);
    ~PathSorter() override;
    PathSorter(const PathSorter&) = delete;
    PathSorter& operator=(const PathSorter&) = delete;
    PathSorter(PathSorter&&) = delete;
    PathSorter& operator=(PathSorter&&) = delete;

    const std::vector<PathEdge>& edges() const noexcept;
    const std::vector<int>& pass_counts() const noexcept;
    int total_passes() const noexcept;
    CPLEXSolveResult solve(double gapTolerance = 0);
    double minimum_distance() const;
    std::vector<OrderedPass> extract_order() const;

private:
    PathSortInstance _instance;

    void generate_variables() override;
    void generate_constraints() override;
    void invalidate_result() override;
    void set_module_objective();

    void set_first_pass_variable(std::size_t edge_index);
    void set_distance_variable(std::size_t from_edge, std::size_t to_edge);
    void set_position_variable(ArcVariables& variables, const string& variable_name,
                               std::size_t edge_index, int position);

    vector<int> _pass_count;
    int _K = 0;

    IloNumVarArray _X;
    ArcVariables _A;
    ArcVariables _D;
    ArcVariables _Z;
    bool _has_solution = false;

};

#endif
