#include <heuristic/WarmStart.h>
#include <model/PathSorterClusters.h>
#include <algorithm>
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

    std::vector<int> cluster_max(_sorter._cluster_count, 1);
    for (const auto& [cluster, edges] : _sorter._edges_by_cluster)
        for (int e : edges)
            cluster_max[cluster] = std::max(
                cluster_max[cluster],
                positions[_sorter._segment_map.at({e, 0})]);

    for (int c = 0; c < _sorter._cluster_count; ++c)
        if (_sorter._edges_by_cluster.count(c) != 0)
            add_value(_sorter._L[c], cluster_max[c]);

    std::vector<std::vector<int>> order_values(
        _sorter._cluster_count,
        std::vector<int>(_sorter._cluster_count, 0));
    std::vector<std::vector<int>> penalty_values(
        _sorter._cluster_count,
        std::vector<int>(_sorter._instance.edges.size(), 0));
    std::vector<std::vector<bool>> used_penalties(
        _sorter._cluster_count,
        std::vector<bool>(_sorter._instance.edges.size(), false));

    for (const auto& [c, c_edges] : _sorter._edges_by_cluster) {
        for (const auto& [d, d_edges] : _sorter._edges_by_cluster) {
            if (c >= d) continue;

            int penalties_if_c_first = 0;
            for (int f : d_edges)
                if (positions[_sorter._segment_map.at({f, 0})] <
                    cluster_max[c])
                    ++penalties_if_c_first;
            int penalties_if_d_first = 0;
            for (int e : c_edges)
                if (positions[_sorter._segment_map.at({e, 0})] <
                    cluster_max[d])
                    ++penalties_if_d_first;

            const double cost_if_c_first =
                static_cast<double>(c_edges.size()) * penalties_if_c_first;
            const double cost_if_d_first =
                static_cast<double>(d_edges.size()) * penalties_if_d_first;
            const bool c_first = cost_if_c_first <= cost_if_d_first;
            order_values[c][d] = c_first ? 1 : 0;
            for (int f : d_edges) used_penalties[c][f] = true;
            for (int e : c_edges) used_penalties[d][e] = true;
            if (c_first) {
                for (int f : d_edges)
                    penalty_values[c][f] =
                        positions[_sorter._segment_map.at({f, 0})] <
                        cluster_max[c];
            } else {
                for (int e : c_edges)
                    penalty_values[d][e] =
                        positions[_sorter._segment_map.at({e, 0})] <
                        cluster_max[d];
            }
        }
    }

    for (int c = 0; c < _sorter._cluster_count; ++c) {
        for (int d = c + 1; d < _sorter._cluster_count; ++d)
            if (_sorter._edges_by_cluster.count(c) != 0 &&
                _sorter._edges_by_cluster.count(d) != 0)
                add_value(_sorter._O[c][d], order_values[c][d]);
        for (int e = 0; e < _sorter._instance.edges.size(); ++e)
            if (used_penalties[c][e])
                add_value(_sorter._Q[c][e], penalty_values[c][e]);
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
        for (const auto& [c, edges] : _sorter._edges_by_cluster) {
            (void)edges;
            capture(_sorter._L[c]);
        }
        for (const auto& [c, c_edges] : _sorter._edges_by_cluster) {
            for (const auto& [d, d_edges] : _sorter._edges_by_cluster) {
                if (c >= d) continue;
                capture(_sorter._O[c][d]);
                for (int f : d_edges) capture(_sorter._Q[c][f]);
                for (int e : c_edges) capture(_sorter._Q[d][e]);
            }
        }

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
