#ifndef PATH_SORTER_FIRST_PASS_H
#define PATH_SORTER_FIRST_PASS_H

#include <model/PathSolver.h>

using namespace std;

class PathSorterFirstPass : public PathSolver {
public:
    PathSorterFirstPass();
    explicit PathSorterFirstPass(PathSortInstance instance);
    ~PathSorterFirstPass() override;
    PathSorterFirstPass(const PathSorterFirstPass&) = delete;
    PathSorterFirstPass& operator=(const PathSorterFirstPass&) = delete;
    PathSorterFirstPass(PathSorterFirstPass&&) = delete;
    PathSorterFirstPass& operator=(PathSorterFirstPass&&) = delete;

    double minimum_distance() const;

private:
    friend struct PathSorterFirstPassTestAccess;

    void generate_variables() override;
    void generate_constraints() override;
    void set_module_objective();

    void set_distance_variable(int e, int s);
    void calculate_deposit_distances();
    vector<int> _dist;
    VariableMatrix _D;
};

#endif
