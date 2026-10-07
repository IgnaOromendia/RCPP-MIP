#ifndef PATH_SORTER_H
#define PATH_SORTER_H

#include <CPLEXSolver.h>
#include <model/PathSortInstance.h>
#include <cstddef>
#include <map>

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
    int total_passes() const noexcept;
    CPLEXSolveResult solve(double gapTolerance = 0);
    double minimum_distance() const;
    std::vector<OrderedPass> extract_order() const;

private:
    friend struct PathSorterTestAccess;

    void generate_variables() override;
    void generate_constraints() override;
    void invalidate_result() override;
    void set_module_objective();

    void set_position_variable(int e, int k);
    void set_distance_variable(int e, int s);
    void set_order_variable(ArcVariables& variables, const string& variable_name, int e, int k);

    void build_segments();
    bool is_transition(int r, int s) const;

    PathSortInstance _instance;

    map<segment, int> _segment_map;
    vector<segment> _segments;
    int _K = 0;

    IloNumVarArray _X;
    ArcVariables _D;
    ArcVariables _Z;
    bool _has_solution = false;

};

#endif
