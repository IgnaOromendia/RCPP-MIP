#ifndef CLUSTER_ASSIGNMENT_READER_H
#define CLUSTER_ASSIGNMENT_READER_H
#include <model/Instance.h>
#include <istream>
#include <string>

class ClusterAssignmentReader {
public:
    static void read(Instance& instance, std::istream& clusters,
                     std::istream* neutral_nodes = nullptr,
                     const std::string& name = "clusters",
                     const std::string& neutral_name = "nodos neutros");
};
#endif
