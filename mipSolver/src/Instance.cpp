#include "../lib/Instance.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

void Instance::validate() const {
    auto require = [](bool valid, const std::string& field) {
        if (!valid) throw std::invalid_argument("Instancia invalida: " + field);
    };
    const auto max_index = std::numeric_limits<int>::max();
    require(vehicles > 0 && vehicles < max_index, "cantidad de vehiculos");
    require(nodes > 0 && nodes <= max_index - 2, "cantidad de nodos");
    require(deposit_nodes.size() <= static_cast<std::size_t>(nodes), "cantidad de adyacentes al deposito");
    // Graph IDs and virtual node IDs are int. Check sums without overflowing.
    require(edges.size() <= static_cast<std::size_t>(max_index) / 4 &&
            arcs.size() <= static_cast<std::size_t>(max_index) / 2,
            "cantidad de tramos");
    require(4 * edges.size() + 2 * arcs.size() < static_cast<std::size_t>(max_index) &&
            edges.size() + arcs.size() + deposit_nodes.size() < static_cast<std::size_t>(max_index),
            "cantidad de tramos");
    auto node_ok = [&](int node) { return node >= 0 && node < nodes; };
    for (std::size_t i = 0; i < deposit_nodes.size(); ++i)
        require(node_ok(deposit_nodes[i]), "adyacente al deposito " + std::to_string(i + 1));
    auto validate_edges = [&](const auto& records, const std::string& kind) {
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto& edge = records[i];
            const auto prefix = kind + " " + std::to_string(i + 1) + ": ";
            require(node_ok(edge.from) && node_ok(edge.to), prefix + "nodos fuera de rango");
            require(edge.zone >= -1 && edge.zone <= vehicles, prefix + "zona fuera de rango");
            require(std::isfinite(edge.cost) && edge.cost > 0, prefix + "costo debe ser positivo y finito");
            require(std::isfinite(edge.demand) && edge.demand >= 0, prefix + "demanda debe ser no negativa y finita");
        }
    };
    validate_edges(edges, "arista");
    validate_edges(arcs, "arco");
    auto validate_turns = [&](const auto& records, const std::string& kind) {
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto& turn = records[i];
            require(node_ok(turn.v) && node_ok(turn.w) && node_ok(turn.u),
                    kind + " " + std::to_string(i + 1) + ": nodos fuera de rango");
        }
    };
    validate_turns(turns, "giro");
    validate_turns(illegal_turns, "giro prohibido");
}
