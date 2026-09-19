#ifndef SOLVE_RESULT_H
#define SOLVE_RESULT_H

#include <ilcplex/ilocplex.h>
#include <optional>
#include <stdexcept>
#include "Solution.h"

// Owns the captured values; no dependency on a live solver or environment.
struct SolveResult {
    bool has_solution = false;
    IloAlgorithm::Status status = IloAlgorithm::Unknown;

    double get_obj_value() const {
        require_solution();
        return _solution->objective;
    }

    Solution extract_solution() const {
        require_solution();
        return *_solution;
    }

private:
    friend class RCPPSolver;
    std::optional<Solution> _solution;

    void require_solution() const {
        if (!has_solution || !_solution) {
            throw std::logic_error("No hay una solucion disponible para consultar o exportar.");
        }
    }
};

#endif
