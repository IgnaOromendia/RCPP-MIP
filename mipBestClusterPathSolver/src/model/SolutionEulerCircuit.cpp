#include <model/SolutionEulerCircuit.h>
#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>

using namespace std;

namespace {
struct PendingArc {
    OrderedSolutionArc arc;
    long long remaining;
};

void append_values(
        vector<PendingArc>& pending,
        const vector<ArcValue<long long>>& values,
        SolutionArcKind kind,
        map<int, long long>& balance,
        unsigned long long& total) {
    for (const ArcValue<long long>& value : values) {
        if (value.value < 0)
            throw invalid_argument("La solucion contiene una multiplicidad negativa");
        if (value.value == 0) continue;
        if (value.vehicle != 1)
            throw invalid_argument(
                "BestClusterPath requiere una solucion global de un solo vehiculo");
        if (value.from == value.to)
            throw invalid_argument("La solucion contiene un arco degenerado");

        pending.push_back({
            {value.id, value.from, value.to, kind}, value.value});
        balance[value.from] += value.value;
        balance[value.to] -= value.value;
        total += static_cast<unsigned long long>(value.value);
        if (total > numeric_limits<size_t>::max())
            throw invalid_argument("La solucion es demasiado grande para ordenar");
    }
}
}

vector<OrderedSolutionArc> SolutionEulerCircuit::build(const Solution& solution) {
    vector<PendingArc> pending;
    map<int, long long> balance;
    unsigned long long total = 0;
    append_values(pending, solution.service, SolutionArcKind::Service, balance, total);
    append_values(pending, solution.traversals, SolutionArcKind::Traversal, balance, total);
    append_values(pending, solution.deposit_traversals, SolutionArcKind::Deposit, balance, total);

    if (pending.empty())
        throw invalid_argument("La solucion global no contiene un recorrido");
    for (const auto& [node, value] : balance) {
        (void) node;
        if (value != 0)
            throw invalid_argument("La solucion global no esta balanceada");
    }

    map<int, vector<size_t>> outgoing;
    for (size_t index = 0; index < pending.size(); ++index)
        outgoing[pending[index].arc.from].push_back(index);
    if (!outgoing.count(-1))
        throw invalid_argument("La solucion global no esta conectada al deposito");

    map<int, size_t> cursor;
    vector<int> node_stack = {-1};
    vector<OrderedSolutionArc> edge_stack;
    vector<OrderedSolutionArc> reversed;
    reversed.reserve(static_cast<size_t>(total));

    while (!node_stack.empty()) {
        const int node = node_stack.back();
        vector<size_t>& candidates = outgoing[node];
        size_t& position = cursor[node];
        while (position < candidates.size() &&
               pending[candidates[position]].remaining == 0)
            ++position;

        if (position < candidates.size()) {
            PendingArc& selected = pending[candidates[position]];
            --selected.remaining;
            node_stack.push_back(selected.arc.to);
            edge_stack.push_back(selected.arc);
            continue;
        }

        node_stack.pop_back();
        if (!edge_stack.empty()) {
            reversed.push_back(edge_stack.back());
            edge_stack.pop_back();
        }
    }

    if (reversed.size() != total)
        throw invalid_argument(
            "La solucion global contiene una componente desconectada del deposito");
    reverse(reversed.begin(), reversed.end());

    int current = -1;
    for (const OrderedSolutionArc& arc : reversed) {
        if (arc.from != current)
            throw invalid_argument("No se pudo ordenar el circuito global");
        current = arc.to;
    }
    if (current != -1)
        throw invalid_argument("El circuito global no termina en el deposito");
    return reversed;
}
