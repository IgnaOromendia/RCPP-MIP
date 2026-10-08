#include <io/PathOrderWriter.h>
#include <fstream>
#include <stdexcept>

namespace {
void write_node(std::ostream& output, int node, int deposit) {
    if (node == deposit) output << 'D';
    else output << node + 1;
}

void write_segments_impl(std::ostream& output,
                         const std::vector<OrderedPass>& order,
                         const std::vector<int>* edge_clusters) {
    output << "vehiculo,orden,nodo_origen,nodo_destino";
    if (edge_clusters != nullptr) output << ",cluster";
    output << '\n';
    for (const OrderedPass& item : order) {
        if (item.edge.original_edge_id < 0) continue;
        if (item.edge.original_from < 0 || item.edge.original_to < 0)
            throw std::logic_error("Falta el mapeo a nodos originales");
        output << item.edge.vehicle << ',' << item.position << ','
               << item.edge.original_from + 1 << ',' << item.edge.original_to + 1;
        if (edge_clusters != nullptr) {
            if (item.edge_index < 0 ||
                item.edge_index >= static_cast<int>(edge_clusters->size()))
                throw std::logic_error("Falta el cluster de una arista ordenada");
            output << ',' << edge_clusters->at(item.edge_index) + 1;
        }
        output << '\n';
    }
    if (!output) throw std::runtime_error("Error al escribir los segmentos de la ruta");
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
    write_segments_impl(output, order, nullptr);
}

void PathOrderWriter::write_cluster_segments(
    std::ostream& output, const std::vector<OrderedPass>& order,
    const std::vector<int>& edge_clusters) {
    write_segments_impl(output, order, &edge_clusters);
}

void PathOrderWriter::write_segments_file(const std::string& path,
                                          const std::vector<OrderedPass>& order) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("Error al abrir " + path);
    write_segments(output, order);
}

void PathOrderWriter::write_cluster_segments_file(
    const std::string& path, const std::vector<OrderedPass>& order,
    const std::vector<int>& edge_clusters) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("Error al abrir " + path);
    write_cluster_segments(output, order, edge_clusters);
}
