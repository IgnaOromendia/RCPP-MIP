#ifndef PATH_SOLVER_H
#define PATH_SOLVER_H

#include <CPLEXSolver.h>
#include <model/OrderedPass.h>
#include <model/PathSortInstance.h>
#include <map>

using namespace std;

class PathSolver : public CPLEXSolver {
public:
    PathSolver();
    explicit PathSolver(PathSortInstance instance);
    ~PathSolver() override;
    PathSolver(const PathSolver&) = delete;
    PathSolver& operator=(const PathSolver&) = delete;
    PathSolver(PathSolver&&) = delete;
    PathSolver& operator=(PathSolver&&) = delete;

    const PathSortInstance& instance() const noexcept;
    const std::vector<PathEdge>& edges() const noexcept;
    int total_passes() const noexcept;
    CPLEXSolveResult solve(double gapTolerance = 0);
    std::vector<OrderedPass> extract_order() const;

protected:
    friend struct PathSorterFirstPassTestAccess;

    void generate_variables() override = 0;
    void generate_constraints() override = 0;
    void invalidate_result() override;
    void set_position_variable(int e, int k);
    void set_order_variable(VariableArray& variables, const string& variable_name,
                            int e, int k);
    void build_segments();
    bool is_transition(int r, int s) const;

    PathSortInstance _instance;
    map<segment, int> _segment_map;
    vector<segment> _segments;
    int _K = 0;
    IloNumVarArray _X;
    VariableArray _Z;
    bool _has_solution = false;
};

#endif
