#include <heuristic/WarmStart.h>
#include <model/PathSorterClusters.h>
#include <cmath>
#include <stdexcept>

WarmStart::WarmStart(PathSorterCluster& sorter): _sorter(sorter) {}

std::vector<int> WarmStart::segment_positions() const {
    std::vector<int> positions(_sorter._segments.size());
    for (int position = 0; position < _sorter._K; ++position)
        positions[_sorter._warm_start_order[position]] = position + 1;
    return positions;
}

void WarmStart::generate() {
    IloNumVarArray variables(_sorter._env);
    IloNumArray values(_sorter._env);

    const auto add_value = [&](IloNumVar variable, IloNum value) {
        variables.add(variable);
        values.add(value);
    };

    const std::vector<int> positions = segment_positions();

    for (int r = 0; r < _sorter._K; ++r)
        add_value(_sorter._X[r], positions[r]);

    for (int r = 0; r < _sorter._K; ++r) {
        const int next = positions[r] < _sorter._K
            ? _sorter._warm_start_order[positions[r]]
            : -1;
        for (int s = 0; s < _sorter._K; ++s)
            if (_sorter.is_transition(r, s))
                add_value(_sorter._Z[r][s], s == next ? 1 : 0);
    }

    const std::string warm_start_name = _sorter._hierholzer_lookahead == 1
        ? "cluster-hierholzer"
        : "cluster-hierholzer-lookahead-" +
              std::to_string(_sorter._hierholzer_lookahead);
    _sorter.addWarmStart(variables, values, warm_start_name);
    values.end();
    variables.end();
}

bool WarmStart::complete_hierholzer() {
    if (_sorter._X.getSize() != _sorter._K)
        throw std::logic_error(
            "Debe generar el modelo antes de completar el warm start");

    const std::vector<int> positions = segment_positions();
    PathSorterCluster::WarmStartBounds original_bounds;
    for (int r = 0; r < _sorter._K; ++r) {
        _sorter.fix_and_save_bounds(
            _sorter._X[r], positions[r], original_bounds);
        const int next = positions[r] < _sorter._K
            ? _sorter._warm_start_order[positions[r]]
            : -1;
        for (int s = 0; s < _sorter._K; ++s)
            if (_sorter.is_transition(r, s))
                _sorter.fix_and_save_bounds(
                    _sorter._Z[r][s], s == next ? 1 : 0,
                    original_bounds);
    }

    CPLEXSolveResult restricted;
    try {
        restricted = _sorter.CPLEXSolver::solve(0);
    } catch (...) {
        _sorter.restore_bounds(original_bounds);
        _sorter.invalidate_result();
        throw;
    }

    if (!restricted.has_solution) {
        _sorter.restore_bounds(original_bounds);
        _sorter.invalidate_result();
        return false;
    }

    try {
        IloNumVarArray variables(_sorter._env);
        IloNumArray values(_sorter._env);
        const auto capture = [&](IloNumVar variable) {
            variables.add(variable);
            const IloNum value = _sorter.get_value(variable);
            values.add(variable.getType() == IloNumVar::Float
                           ? value
                           : std::round(value));
        };

        for (int r = 0; r < _sorter._K; ++r)
            capture(_sorter._X[r]);
        for (int r = 0; r < _sorter._K; ++r)
            for (int s = 0; s < _sorter._K; ++s)
                if (_sorter.is_transition(r, s))
                    capture(_sorter._Z[r][s]);
        _sorter.restore_bounds(original_bounds);
        const int mip_start_count = _sorter.get_mip_start_count();
        if (mip_start_count > 0)
            _sorter._solver.deleteMIPStarts(0, mip_start_count);
        _sorter.addWarmStart(
            variables, values, "cluster-hierholzer-restricted-solve");
        values.end();
        variables.end();
    } catch (...) {
        _sorter.restore_bounds(original_bounds);
        _sorter.invalidate_result();
        throw;
    }
    _sorter.invalidate_result();
    return true;
}
