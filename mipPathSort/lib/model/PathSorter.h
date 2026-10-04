#ifndef PATH_SORTER_H
#define PATH_SORTER_H

#include <CPLEXSolver.h>
#include "PathSortInstance.h"

using namespace std;

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

private:
    PathSortInstance _instance;

    void generate_variables() override;
	void generate_constraints() override;

    void set_pass_variable(ArcVariables& variables, std::size_t edge_index, const string& variable_name, int pass);
    void set_distance_variable(std::size_t from_edge, std::size_t to_edge);
    void set_position_variable(std::size_t edge_index, int pass, int position);

    vector<int> _pass_count;
    int _K = 0;

    ArcVariables _X;
    ArcVariables _D;
    ArrayArcVariables _Z;

};

#endif
