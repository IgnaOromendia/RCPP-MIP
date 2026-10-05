#include "../../lib/io/PathSortInstanceReader.h"
#include <graph/Graph.h>
#include <graph/SuperGraph.h>
#include <io/InstanceReader.h>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>

namespace {
enum class Section { none, service, traversals, deposit_traversals, ignored };

struct ParsedVariable {
    char kind;
    int from, to, vehicle;
    long long value;
};

std::string trim(const std::string& value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}

int parse_positive_int(const std::string& token, const std::string& context) {
    std::size_t consumed = 0;
    long long value;
    try {
        value = std::stoll(token, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument(context + ": entero invalido");
    }
    if (consumed != token.size() || value <= 0 || value > std::numeric_limits<int>::max())
        throw std::invalid_argument(context + ": entero fuera de rango");
    return static_cast<int>(value);
}

int parse_node(const std::string& token, const std::string& context) {
    if (token == "D") return -1;
    return parse_positive_int(token, context) - 1;
}

ParsedVariable parse_variable(const std::string& line, const std::string& context) {
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos || line.find('=', equals + 1) != std::string::npos)
        throw std::invalid_argument(context + ": variable invalida");

    const std::string name = trim(line.substr(0, equals));
    const std::string raw_value = trim(line.substr(equals + 1));
    std::vector<std::string> parts;
    std::size_t begin = 0;
    while (true) {
        const std::size_t separator = name.find('_', begin);
        parts.push_back(name.substr(begin, separator - begin));
        if (separator == std::string::npos) break;
        begin = separator + 1;
    }
    if (parts.size() != 4 || (parts[0] != "X" && parts[0] != "Y"))
        throw std::invalid_argument(context + ": nombre de variable invalido");

    std::size_t consumed = 0;
    long long value;
    try {
        value = std::stoll(raw_value, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument(context + ": multiplicidad invalida");
    }
    if (consumed != raw_value.size() || value < 0)
        throw std::invalid_argument(context + ": multiplicidad debe ser un entero no negativo");

    return {parts[0][0], parse_node(parts[1], context), parse_node(parts[2], context),
            parse_positive_int(parts[3], context), value};
}
}

PathSortInstance PathSortInstanceReader::read(const Instance& graph_instance,
                                              std::istream& solution,
                                              const std::string& solution_name) {
    const Graph graph(graph_instance);
    const SuperGraph super_graph(graph, graph_instance.turns, graph_instance.illegal_turns);
    using Counts = std::pair<long long, long long>; // X, Y
    std::vector<std::vector<Counts>> counts(
        super_graph.arcs_amount(), std::vector<Counts>(graph_instance.vehicles + 1));
    std::map<std::pair<int, int>, int> arc_by_endpoints;
    for (const SuperArc& arc : super_graph.arcs()) {
        const int from = arc.from == super_graph.deposit() ? -1 : arc.from;
        const int to = arc.to == super_graph.deposit() ? -1 : arc.to;
        if (!arc_by_endpoints.emplace(std::make_pair(from, to), arc.id).second)
            throw std::logic_error("El supergrafo contiene arcos indistinguibles en out.dat");
    }

    std::map<std::tuple<char, int, int>, bool> seen;
    Section section = Section::none;
    std::string line;
    int line_number = 0;
    while (std::getline(solution, line)) {
        ++line_number;
        const std::string value = trim(line);
        if (value.empty() || value.rfind("OBJ:", 0) == 0) continue;
        if (value == "---- X ----") { section = Section::service; continue; }
        if (value == "---- Y ----") { section = Section::traversals; continue; }
        if (value == "---- YDK & YKD ----") { section = Section::deposit_traversals; continue; }
        if (value == "---- F ----" || value == "---- FDK ----") {
            section = Section::ignored;
            continue;
        }
        if (section == Section::ignored) continue;
        if (section == Section::none)
            throw std::invalid_argument(solution_name + ": linea " + std::to_string(line_number) +
                                        ": contenido fuera de una seccion conocida");

        const std::string context = solution_name + ": linea " + std::to_string(line_number);
        const ParsedVariable variable = parse_variable(value, context);
        const char expected_kind = section == Section::service ? 'X' : 'Y';
        if (variable.kind != expected_kind)
            throw std::invalid_argument(context + ": variable en una seccion incorrecta");
        if (variable.vehicle > graph_instance.vehicles)
            throw std::invalid_argument(context + ": vehiculo fuera de rango");

        const auto found = arc_by_endpoints.find({variable.from, variable.to});
        if (found == arc_by_endpoints.end())
            throw std::invalid_argument(context + ": arco inexistente en la Instance");
        const SuperArc& arc = *super_graph.super_arc_with_id(found->second);
        const bool deposit_variable = section == Section::deposit_traversals;
        if ((arc.edge_id == -2) != deposit_variable)
            throw std::invalid_argument(context + ": arco en una seccion incorrecta");
        if (section == Section::service && !arc.requested)
            throw std::invalid_argument(context + ": X corresponde a un arco no requerido");

        const auto key = std::make_tuple(variable.kind, arc.id, variable.vehicle);
        if (!seen.emplace(key, true).second)
            throw std::invalid_argument(context + ": variable duplicada");
        Counts& arc_counts = counts[arc.id][variable.vehicle];
        const long long other_count = section == Section::service
            ? arc_counts.second : arc_counts.first;
        if (variable.value > std::numeric_limits<long long>::max() - other_count)
            throw std::invalid_argument(context + ": multiplicidad total fuera de rango");
        if (section == Section::service) arc_counts.first = variable.value;
        else arc_counts.second = variable.value;
    }
    if (solution.bad()) throw std::runtime_error(solution_name + ": error de lectura");

    PathSortInstance result;
    result.vehicles = graph_instance.vehicles;
    result.deposit = super_graph.deposit();
    result.adj.resize(super_graph.nodes_amount() + 1);
    for (const SuperArc& arc : super_graph.arcs()) {
        bool selected = false;
        for (int vehicle = 1; vehicle <= graph_instance.vehicles; ++vehicle) {
            const Counts& arc_counts = counts[arc.id][vehicle];
            if (arc_counts.first == 0 && arc_counts.second == 0) continue;
            selected = true;
            result.edges.push_back({
                arc.id,
                arc.edge_id,
                arc.from,
                arc.to,
                vehicle,
                arc_counts.first,
                arc_counts.second
            });
        }
        if (selected) result.adj[arc.from].emplace_back(arc.to, arc.id);
    }
    return result;
}

PathSortInstance PathSortInstanceReader::read_file(const Instance& graph_instance,
                                                   const std::string& solution_path) {
    std::ifstream solution(solution_path);
    if (!solution) throw std::runtime_error("Error al abrir " + solution_path);
    return read(graph_instance, solution, solution_path);
}

PathSortInstance PathSortInstanceReader::read_files(const std::string& graph_path,
                                                    const std::string& turns_path,
                                                    const std::string& solution_path) {
    return read_file(InstanceReader::read_files(graph_path, turns_path), solution_path);
}
