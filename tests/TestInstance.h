#ifndef TEST_INSTANCE_H
#define TEST_INSTANCE_H

#include "lib/Graph.h"
#include "lib/SuperGraph.h"
#include "lib/InstanceReader.h"
#include <filesystem>

inline Instance test_instance(const std::filesystem::path& graph) {
    return InstanceReader::read_files(graph.string(), (graph.parent_path() / "turns.dat").string());
}

inline SuperGraph test_super_graph(const Instance& instance) {
    const Graph graph(instance);
    return SuperGraph(graph, instance.turns, instance.illegal_turns);
}

#endif
