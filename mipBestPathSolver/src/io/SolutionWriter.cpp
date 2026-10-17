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

void SolutionWriter::write_file(const std::string& path, const Solution& solution) {
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(path);
    write(output, solution);
    output.close();
}
