#include <model/SolutionEulerCircuit.h>
#include <algorithm>
#include <sstream>
#include <stdexcept>

using namespace std;

SolutionEulerCircuit::SolutionEulerCircuit(int node_count, const Solution& solution)
        : segment_adj(node_count + 1), deposit(node_count) {
    add_pending_segments(solution.service, SolutionArcKind::Service);
    add_pending_segments(solution.traversals, SolutionArcKind::Traversal);
    add_pending_segments(solution.deposit_traversals, SolutionArcKind::Deposit);
}

vector<ClusterSolutionArc> SolutionEulerCircuit::circuit() {
    vector<int> node_stack = {deposit};
    vector<ClusterSolutionArc> edge_stack;
    vector<ClusterSolutionArc> reversed;
    reversed.reserve(total);

    while (!node_stack.empty()) {
        const int node = node_stack.back();
        vector<int>& candidates = segment_adj[node];

        while (!candidates.empty() and pending[candidates.back()].remaining_passes == 0)
            candidates.pop_back();

        if (!candidates.empty()) {
            ClusterSolutionArc& current = pending[candidates.back()];
            --current.remaining_passes;
            if (current.remaining_passes == 0)
                candidates.pop_back();
            node_stack.push_back(current.to == -1 ? deposit : current.to);
            edge_stack.push_back(current);
            continue;
        }

        node_stack.pop_back();
        if (!edge_stack.empty()) {
            reversed.push_back(edge_stack.back());
            edge_stack.pop_back();
        }
    }

    for (const ClusterSolutionArc& value : pending) {
        if (value.remaining_passes == 0) continue;
        const char* kind = value.kind == SolutionArcKind::Service ? "servicio" :
            value.kind == SolutionArcKind::Deposit ? "deposito" : "recorrido";
        ostringstream detail;
        detail << "La solucion global contiene una componente desconectada del deposito: "
               << kind << " id=" << value.id
               << " (" << value.from << " -> " << value.to << ")"
               << ", pasadas_restantes=" << value.remaining_passes;
        throw invalid_argument(detail.str());
    }

    reverse(reversed.begin(), reversed.end());
    int previous = -1;
    for (const auto& segment : reversed) {
        if (segment.from != previous)
            throw invalid_argument("La solucion no conserva balance ni continuidad");
        previous = segment.to;
    }
    if (previous != -1) throw invalid_argument("El circuito no termina en el deposito");
    return reversed;
}

void SolutionEulerCircuit::add_pending_segments(const vector<ArcValue<long long>> &values, SolutionArcKind kind) {
    for (const ArcValue<long long>& value : values) {
        if (value.value < 0 || value.from < -1 || value.from >= deposit ||
            value.to < -1 || value.to >= deposit)
            throw invalid_argument("Multiplicidad o extremo de solucion invalido");
        if (value.value == 0) continue;
        pending.emplace_back(value, kind);
        int from = value.from == -1 ? deposit : value.from;
        segment_adj[from].push_back(pending.size() - 1);
        total += value.value;
    }
}
