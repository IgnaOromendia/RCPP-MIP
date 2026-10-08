#include "../../lib/io/PathOrderWriter.h"
#include <fstream>
#include <stdexcept>

namespace {
void write_node(std::ostream& output, int node, int deposit) {
    if (node == deposit) output << 'D';
    else output << node + 1;
}
}

void PathOrderWriter::write(std::ostream& output, const std::vector<OrderedPass>& order,
                            int deposit) {
    output << "posicion vehiculo origen destino pasada super_arco arista_original\n";
    for (const OrderedPass& item : order) {
        output << item.position << ' ' << item.edge.vehicle << ' ';
        write_node(output, item.edge.from, deposit);
        output << ' ';
        write_node(output, item.edge.to, deposit);
        output << ' ' << item.pass << ' ' << item.edge.super_arc_id << ' '
               << item.edge.original_edge_id << '\n';
    }
    if (!output) throw std::runtime_error("Error al escribir el orden de la solucion");
}

void PathOrderWriter::write_file(const std::string& path,
                                 const std::vector<OrderedPass>& order,
                                 int deposit) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("Error al abrir " + path);
    write(output, order, deposit);
}

void PathOrderWriter::write_segments(std::ostream& output,
                                     const std::vector<OrderedPass>& order) {
    output << "vehiculo,orden,nodo_origen,nodo_destino\n";
    for (const OrderedPass& item : order) {
        if (item.edge.original_edge_id < 0) continue;
        if (item.edge.original_from < 0 || item.edge.original_to < 0)
            throw std::logic_error("Falta el mapeo a nodos originales");
        output << item.edge.vehicle << ',' << item.position << ','
               << item.edge.original_from + 1 << ',' << item.edge.original_to + 1 << '\n';
    }
    if (!output) throw std::runtime_error("Error al escribir los segmentos de la ruta");
}

void PathOrderWriter::write_segments_file(const std::string& path,
                                          const std::vector<OrderedPass>& order) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("Error al abrir " + path);
    write_segments(output, order);
}
