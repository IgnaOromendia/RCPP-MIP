#include "../../lib/io/SolutionWriter.h"
#include <fstream>
#include <stdexcept>

namespace {
std::string node_name(int node) {
    return node == -1 ? "D" : std::to_string(node + 1);
}

template<class Value>
void write_values(std::ostream& output, const std::vector<ArcValue<Value>>& values, const char* name) {
    for (const auto& value : values)
        output << name << '_' << node_name(value.from) << '_' << node_name(value.to)
               << '_' << value.vehicle << " = " << value.value << '\n';
}

void write_edge_values(
        std::ostream& output,
        const std::map<int, long long>& values,
        const char* name) {
    for (const auto& [edge_id, value] : values)
        output << name << '_' << edge_id + 1 << " = " << value << '\n';
}

std::filesystem::path output_path(
        int nodes,
        const std::filesystem::path& path,
        const std::string& filename_prefix) {
    return path.empty()
        ? std::filesystem::path("data") / std::to_string(nodes) /
            (filename_prefix + "_" + std::to_string(nodes) + ".dat")
        : path;
}
}

void SolutionWriter::write(std::ostream& output, const Solution& solution) {
    output << "OBJ: " << solution.objective << "\n\n---- X ----\n";
    write_values(output, solution.service, "X");
    output << "\n---- Y ----\n";
    write_values(output, solution.traversals, "Y");
    output << "\n---- YDK & YKD ----\n";
    write_values(output, solution.deposit_traversals, "Y");
    output << "\n---- F ----\n";
    write_values(output, solution.flow, "F");
    output << "\n---- FDK ----\n";
    write_values(output, solution.deposit_flow, "F");
    if (!output) throw std::runtime_error("Error al escribir la solucion");
}

void SolutionWriter::write(std::ostream& output, const GraphSolution& solution) {
    output << "OBJ: " << solution.objective << "\n\n---- X ----\n";
    write_edge_values(output, solution.service, "X");
    output << "\n---- Y ----\n";
    write_edge_values(output, solution.traversals, "Y");
    if (!output) throw std::runtime_error("Error al escribir la solucion");
}

void SolutionWriter::write_file(const std::string& path, const Solution& solution) {
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(path);
    write(output, solution);
    output.close();
}

void SolutionWriter::write_file(const std::string& path, const GraphSolution& solution) {
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(path);
    write(output, solution);
    output.close();
}

std::filesystem::path SolutionWriter::write_output(
    const Solution& solution,
    int nodes,
    const std::filesystem::path& path,
    const std::string& filename_prefix) {
    const std::filesystem::path destination = output_path(nodes, path, filename_prefix);
    if (destination.has_parent_path())
        std::filesystem::create_directories(destination.parent_path());
    write_file(destination.string(), solution);
    return destination;
}

std::filesystem::path SolutionWriter::write_output(
    const GraphSolution& solution,
    int nodes,
    const std::filesystem::path& path,
    const std::string& filename_prefix) {
    const std::filesystem::path destination = output_path(nodes, path, filename_prefix);
    if (destination.has_parent_path())
        std::filesystem::create_directories(destination.parent_path());
    write_file(destination.string(), solution);
    return destination;
}
