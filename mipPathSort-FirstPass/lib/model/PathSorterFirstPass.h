#ifndef PATH_SORTER_FIRST_PASS_H
#define PATH_SORTER_FIRST_PASS_H

#include <CPLEXSolver.h>
#include <model/OrderedPass.h>
#include <model/PathSortInstance.h>
#include <cstddef>
#include <map>

using namespace std;

class PathSorterFirstPass : public CPLEXSolver {
public:
    PathSorterFirstPass();
    explicit PathSorterFirstPass(PathSortInstance instance);
    ~PathSorterFirstPass() override;
    PathSorterFirstPass(const PathSorterFirstPass&) = delete;
    PathSorterFirstPass& operator=(const PathSorterFirstPass&) = delete;
    PathSorterFirstPass(PathSorterFirstPass&&) = delete;
    PathSorterFirstPass& operator=(PathSorterFirstPass&&) = delete;

    const std::vector<PathEdge>& edges() const noexcept;
    int total_passes() const noexcept;
    CPLEXSolveResult solve(double gapTolerance = 0);
    double minimum_distance() const;
    std::vector<OrderedPass> extract_order() const;

private:
    friend struct PathSorterFirstPassTestAccess;

    void generate_variables() override;
    void generate_constraints() override;
    void invalidate_result() override;
    void set_module_objective();

    void set_position_variable(int e, int k);
    void set_distance_variable(int e, int s);
    void set_order_variable(ArcVariables& variables, const string& variable_name, int e, int k);

    void build_segments();
    void calculate_deposit_distances();
    bool is_transition(int r, int s) const;

    PathSortInstance _instance;

    map<segment, int> _segment_map;
    vector<segment> _segments;
    vector<int> _dist;
    int _K = 0;

    IloNumVarArray _X;
    ArcVariables _D;
    ArcVariables _Z;
    bool _has_solution = false;

};

#endif
