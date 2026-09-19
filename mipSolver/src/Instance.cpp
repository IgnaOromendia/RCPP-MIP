#include "../lib/Instance.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool valid, const std::string& field) {
    if (!valid) throw std::invalid_argument("Instancia invalida: " + field);
}

bool is_valid_node(int node, int node_count) {
    return node >= 0 && node < node_count;
}

void validate_edge(const InstanceEdge& edge, int node_count, int vehicle_count,
                   const std::string& context) {
    require(is_valid_node(edge.from, node_count) && is_valid_node(edge.to, node_count),
            context + ": nodos fuera de rango");
    require(edge.zone >= -1 && edge.zone <= vehicle_count, context + ": zona fuera de rango");
    require(std::isfinite(edge.cost) && edge.cost > 0,
            context + ": costo debe ser positivo y finito");
    require(std::isfinite(edge.demand) && edge.demand >= 0,
            context + ": demanda debe ser no negativa y finita");
}

void validate_turn(const Turn& turn, int node_count, const std::string& context) {
    require(is_valid_node(turn.v, node_count) && is_valid_node(turn.w, node_count) &&
            is_valid_node(turn.u, node_count), context + ": nodos fuera de rango");
}
}

void Instance::validate() const {
    // Cantidades de la cabecera y espacio para los nodos sinteticos.
    const int max_index = std::numeric_limits<int>::max();
    require(vehicles > 0 && vehicles < max_index, "cantidad de vehiculos");
    require(nodes > 0 && nodes <= max_index - 2, "cantidad de nodos");
    require(deposit_nodes.size() <= static_cast<std::size_t>(nodes), "cantidad de adyacentes al deposito");

    // Limitar las cantidades antes de calcular las sumas evita desbordamientos.
    const std::size_t index_limit = static_cast<std::size_t>(max_index);
    require(edges.size() <= index_limit / 4 && arcs.size() <= index_limit / 2,
            "cantidad de tramos");

    // Cada arista genera 4 nodos virtuales y cada arco, 2. Los IDs son int.
    const std::size_t virtual_node_count = 4 * edges.size() + 2 * arcs.size();
    const std::size_t total_edge_count = edges.size() + arcs.size() + deposit_nodes.size();
    require(virtual_node_count < index_limit && total_edge_count < index_limit, "cantidad de tramos");

    // Validar cada seccion en el mismo orden que el archivo de entrada.
    for (std::size_t i = 0; i < deposit_nodes.size(); ++i) {
        require(is_valid_node(deposit_nodes[i], nodes),
                "adyacente al deposito " + std::to_string(i + 1));
    }
    for (std::size_t i = 0; i < edges.size(); ++i) {
        validate_edge(edges[i], nodes, vehicles, "arista " + std::to_string(i + 1));
    }
    for (std::size_t i = 0; i < arcs.size(); ++i) {
        validate_edge(arcs[i], nodes, vehicles, "arco " + std::to_string(i + 1));
    }
    for (std::size_t i = 0; i < turns.size(); ++i) {
        validate_turn(turns[i], nodes, "giro " + std::to_string(i + 1));
    }
    for (std::size_t i = 0; i < illegal_turns.size(); ++i) {
        validate_turn(illegal_turns[i], nodes, "giro prohibido " + std::to_string(i + 1));
    }
}
