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
